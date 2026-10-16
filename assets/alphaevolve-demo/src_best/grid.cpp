// grid.cpp -- Cartesian grid and halo exchanges for FP64, FP32, and FP16.

#include "grid.h"

#include <cstring>
#include <string>
#include <vector>

bool Grid::create(const Params& p, MPI_Comm comm, std::string& err) {
    destroy();

    int worldRank = 0;
    int worldSize = 1;
    MPI_Comm_rank(comm, &worldRank);
    MPI_Comm_size(comm, &worldSize);

    int dims[2] = {p.px, p.py};
    if (p.px > 0 && p.py > 0) {
        if (p.px * p.py != worldSize) {
            err = "px * py = " + std::to_string(p.px * p.py) + " does not match the " +
                  std::to_string(worldSize) + " MPI ranks";
            return false;
        }
    } else {
        MPI_Dims_create(worldSize, 2, dims);
    }

    if (p.nx % dims[0] != 0 || p.ny % dims[1] != 0) {
        err = "mesh " + std::to_string(p.nx) + "x" + std::to_string(p.ny) +
              " is not divisible by the process grid " + std::to_string(dims[0]) + "x" +
              std::to_string(dims[1]) + "; choose px/py or a different rank count";
        return false;
    }

    const int nxLocal = p.nx / dims[0];
    const int nyLocal = p.ny / dims[1];
    if (nxLocal < NGUARD || nyLocal < NGUARD) {
        err = "local block " + std::to_string(nxLocal) + "x" + std::to_string(nyLocal) +
              " is smaller than NGUARD = " + std::to_string(NGUARD) +
              "; use fewer ranks or a finer mesh";
        return false;
    }

    const int periods[2] = {0, 0};
    MPI_Cart_create(comm, 2, dims, periods, 0, &cart_);
    if (cart_ == MPI_COMM_NULL) {
        err = "MPI_Cart_create returned MPI_COMM_NULL";
        return false;
    }

    MPI_Comm_rank(cart_, &rank_);
    MPI_Comm_size(cart_, &nprocs_);
    MPI_Cart_coords(cart_, rank_, 2, coords_);
    MPI_Cart_shift(cart_, 0, 1, &nbrXlo_, &nbrXhi_);
    MPI_Cart_shift(cart_, 1, 1, &nbrYlo_, &nbrYhi_);

    dims_[0] = dims[0];
    dims_[1] = dims[1];
    ng_ = NGUARD;
    nxg_ = p.nx;
    nyg_ = p.ny;
    nx_ = nxLocal;
    ny_ = nyLocal;
    i0_ = coords_[0] * nxLocal;
    j0_ = coords_[1] * nyLocal;
    sx_ = nx_ + 2 * ng_;
    sy_ = ny_ + 2 * ng_;

    xmin_ = p.xmin;
    ymin_ = p.ymin;
    dx_ = (p.xmax - p.xmin) / static_cast<double>(p.nx);
    dy_ = (p.ymax - p.ymin) / static_cast<double>(p.ny);

    return true;
}

void Grid::destroy() {
    if (cart_ == MPI_COMM_NULL) return;
    int finalized = 0;
    MPI_Finalized(&finalized);
    if (!finalized) MPI_Comm_free(&cart_);
    cart_ = MPI_COMM_NULL;
}

Grid::~Grid() { destroy(); }

void Grid::exchangeX(std::vector<double>& f) const {
    const int count = ng_ * ny_;
    std::vector<double> sendLo(count), sendHi(count), recvLo(count), recvHi(count);

    for (int j = 0; j < ny_; ++j) {
        for (int k = 0; k < ng_; ++k) {
            sendLo[j * ng_ + k] = f[index(k, j)];
            sendHi[j * ng_ + k] = f[index(nx_ - ng_ + k, j)];
        }
    }

    MPI_Sendrecv(sendLo.data(), count, MPI_DOUBLE, nbrXlo_, 100, recvHi.data(), count, MPI_DOUBLE,
                 nbrXhi_, 100, cart_, MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendHi.data(), count, MPI_DOUBLE, nbrXhi_, 101, recvLo.data(), count, MPI_DOUBLE,
                 nbrXlo_, 101, cart_, MPI_STATUS_IGNORE);

    for (int j = 0; j < ny_; ++j) {
        for (int k = 0; k < ng_; ++k) {
            f[index(-ng_ + k, j)] = atWallXlo() ? f[index(0, j)] : recvLo[j * ng_ + k];
            f[index(nx_ + k, j)] = atWallXhi() ? f[index(nx_ - 1, j)] : recvHi[j * ng_ + k];
        }
    }
}

void Grid::exchangeY(std::vector<double>& f) const {
    const int count = ng_ * sx_;
    std::vector<double> recvLo(count), recvHi(count);

    const double* sendLo = &f[index(-ng_, 0)];
    const double* sendHi = &f[index(-ng_, ny_ - ng_)];

    MPI_Sendrecv(sendLo, count, MPI_DOUBLE, nbrYlo_, 200, recvHi.data(), count, MPI_DOUBLE, nbrYhi_,
                 200, cart_, MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendHi, count, MPI_DOUBLE, nbrYhi_, 201, recvLo.data(), count, MPI_DOUBLE, nbrYlo_,
                 201, cart_, MPI_STATUS_IGNORE);

    for (int k = 0; k < ng_; ++k) {
        for (int i = -ng_; i < nx_ + ng_; ++i) {
            const int c = k * sx_ + (i + ng_);
            f[index(i, -ng_ + k)] = atWallYlo() ? f[index(i, 0)] : recvLo[c];
            f[index(i, ny_ + k)] = atWallYhi() ? f[index(i, ny_ - 1)] : recvHi[c];
        }
    }
}

void Grid::exchange(std::vector<double>& f) const {
    exchangeX(f);
    exchangeY(f);
}

void Grid::exchangeX_f(std::vector<float>& f) const {
    const int count = ng_ * ny_;
    std::vector<float> sendLo(count), sendHi(count), recvLo(count), recvHi(count);

    for (int j = 0; j < ny_; ++j) {
        for (int k = 0; k < ng_; ++k) {
            sendLo[j * ng_ + k] = f[index(k, j)];
            sendHi[j * ng_ + k] = f[index(nx_ - ng_ + k, j)];
        }
    }

    MPI_Sendrecv(sendLo.data(), count, MPI_FLOAT, nbrXlo_, 110, recvHi.data(), count, MPI_FLOAT,
                 nbrXhi_, 110, cart_, MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendHi.data(), count, MPI_FLOAT, nbrXhi_, 111, recvLo.data(), count, MPI_FLOAT,
                 nbrXlo_, 111, cart_, MPI_STATUS_IGNORE);

    for (int j = 0; j < ny_; ++j) {
        for (int k = 0; k < ng_; ++k) {
            f[index(-ng_ + k, j)] = atWallXlo() ? f[index(0, j)] : recvLo[j * ng_ + k];
            f[index(nx_ + k, j)] = atWallXhi() ? f[index(nx_ - 1, j)] : recvHi[j * ng_ + k];
        }
    }
}

void Grid::exchangeY_f(std::vector<float>& f) const {
    const int count = ng_ * sx_;
    std::vector<float> recvLo(count), recvHi(count);

    const float* sendLo = &f[index(-ng_, 0)];
    const float* sendHi = &f[index(-ng_, ny_ - ng_)];

    MPI_Sendrecv(sendLo, count, MPI_FLOAT, nbrYlo_, 210, recvHi.data(), count, MPI_FLOAT, nbrYhi_,
                 210, cart_, MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendHi, count, MPI_FLOAT, nbrYhi_, 211, recvLo.data(), count, MPI_FLOAT, nbrYlo_,
                 211, cart_, MPI_STATUS_IGNORE);

    for (int k = 0; k < ng_; ++k) {
        for (int i = -ng_; i < nx_ + ng_; ++i) {
            const int c = k * sx_ + (i + ng_);
            f[index(i, -ng_ + k)] = atWallYlo() ? f[index(i, 0)] : recvLo[c];
            f[index(i, ny_ + k)] = atWallYhi() ? f[index(i, ny_ - 1)] : recvHi[c];
        }
    }
}

void Grid::exchange_f(std::vector<float>& f) const {
    exchangeX_f(f);
    exchangeY_f(f);
}

void Grid::exchangeX_f16(std::vector<_Float16>& f) const {
    const int count = ng_ * ny_;
    std::vector<uint16_t> sendLo(count), sendHi(count), recvLo(count), recvHi(count);

    for (int j = 0; j < ny_; ++j) {
        for (int k = 0; k < ng_; ++k) {
            std::memcpy(&sendLo[j * ng_ + k], &f[index(k, j)], sizeof(uint16_t));
            std::memcpy(&sendHi[j * ng_ + k], &f[index(nx_ - ng_ + k, j)], sizeof(uint16_t));
        }
    }

    MPI_Sendrecv(sendLo.data(), count, MPI_UINT16_T, nbrXlo_, 120, recvHi.data(), count,
                 MPI_UINT16_T, nbrXhi_, 120, cart_, MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendHi.data(), count, MPI_UINT16_T, nbrXhi_, 121, recvLo.data(), count,
                 MPI_UINT16_T, nbrXlo_, 121, cart_, MPI_STATUS_IGNORE);

    for (int j = 0; j < ny_; ++j) {
        for (int k = 0; k < ng_; ++k) {
            if (atWallXlo()) {
                f[index(-ng_ + k, j)] = f[index(0, j)];
            } else {
                std::memcpy(&f[index(-ng_ + k, j)], &recvLo[j * ng_ + k], sizeof(uint16_t));
            }
            if (atWallXhi()) {
                f[index(nx_ + k, j)] = f[index(nx_ - 1, j)];
            } else {
                std::memcpy(&f[index(nx_ + k, j)], &recvHi[j * ng_ + k], sizeof(uint16_t));
            }
        }
    }
}

void Grid::exchangeY_f16(std::vector<_Float16>& f) const {
    const int count = ng_ * sx_;
    std::vector<uint16_t> recvLo(count), recvHi(count);

    const _Float16* sendLo = &f[index(-ng_, 0)];
    const _Float16* sendHi = &f[index(-ng_, ny_ - ng_)];

    MPI_Sendrecv(sendLo, count, MPI_UINT16_T, nbrYlo_, 220, recvHi.data(), count, MPI_UINT16_T,
                 nbrYhi_, 220, cart_, MPI_STATUS_IGNORE);
    MPI_Sendrecv(sendHi, count, MPI_UINT16_T, nbrYhi_, 221, recvLo.data(), count, MPI_UINT16_T,
                 nbrYlo_, 221, cart_, MPI_STATUS_IGNORE);

    for (int k = 0; k < ng_; ++k) {
        for (int i = -ng_; i < nx_ + ng_; ++i) {
            const int c = k * sx_ + (i + ng_);
            if (atWallYlo()) {
                f[index(i, -ng_ + k)] = f[index(i, 0)];
            } else {
                std::memcpy(&f[index(i, -ng_ + k)], &recvLo[c], sizeof(uint16_t));
            }
            if (atWallYhi()) {
                f[index(i, ny_ + k)] = f[index(i, ny_ - 1)];
            } else {
                std::memcpy(&f[index(i, ny_ + k)], &recvHi[c], sizeof(uint16_t));
            }
        }
    }
}

void Grid::exchange_f16(std::vector<_Float16>& f) const {
    exchangeX_f16(f);
    exchangeY_f16(f);
}
