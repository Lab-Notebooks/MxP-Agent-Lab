export const meta = {
  name: 'myloop',
  description: 'Author-reviewer loop: carry out a task spec with a dev/workflows/ workflow (verify | evaluator) until a reviewer approves',
  whenToUse: 'Pass a task spec (string, runs verify) or {task, workflow: "verify"|"evaluator", outdir, maxRounds}. verify = analysis of a run; evaluator = embed an EVOLVE-BLOCK and build an AlphaEvolve experiment. AlphaEvolve runs themselves are triggered separately via dev/workflows/alphaevolve/run.py. For verify, save the returned analysis_markdown to <outdir>/verify/analysis.md (workflow subagents return reports as text).',
  phases: [
    { title: 'Author', detail: 'carry out the task with the chosen dev/workflows/<workflow>' },
    { title: 'Review', detail: 'independently re-check the result against the task spec and the workflow checklist' },
  ],
}

// ---- arguments ---------------------------------------------------------------
// args: "task spec text"                       (workflow defaults to verify)
//    or {task, workflow: "verify" | "evaluator", outdir, maxRounds: 3}
const A = typeof args === 'string' ? { task: args } : (args || {})
if (!A.task) throw new Error('myloop: pass a task specification (string or {task: ...})')
const WORKFLOW = A.workflow || 'verify'
const MAX_ROUNDS = A.maxRounds || 3
const WF_DIR = `dev/workflows/${WORKFLOW}`

const COMMON = `
Shared context:
- Build with \`make\` (src/advect.cpp goes through clang + RAPTOR); run with \`mpirun --oversubscribe -np N ./bubble <params.ini> key=value ...\`. The \`precision\` modes and the FP64 shadow trajectory are described in tools/README_precision.md.
- Variable placement: dfun/phi, rhs, dfun_ref, err sit at cell centres; solver u and v sit at MAC face centres (u(i,j) = low face i-1/2, v(i,j) = low face j-1/2); the .vti u,v are face averages moved to the centre.
- Report only numbers that come from files you produced or read in this run, never from memory.`

// ---- per-workflow profiles ----------------------------------------------------

const ISSUES = {
  type: 'array',
  items: {
    type: 'object',
    properties: {
      severity: { type: 'string', enum: ['blocker', 'major', 'minor'] },
      issue: { type: 'string' },
      fix: { type: 'string', description: 'what the author must do' },
    },
    required: ['severity', 'issue', 'fix'],
  },
}

const REVIEW_SCHEMA = {
  type: 'object',
  properties: {
    approved: { type: 'boolean' },
    issues: ISSUES,
    checks_run: { type: 'array', items: { type: 'string' }, description: 'each check performed and its outcome' },
    summary: { type: 'string' },
  },
  required: ['approved', 'issues', 'checks_run', 'summary'],
}

const PROFILES = {
  verify: {
    rules: `
Rules (verify):
- Do not modify src/, tests/, params/*.ini or Makefile. Running the simulation and verify.py (which write into the run output directory, or a --dest you choose) is expected, not an edit. Only if the task needs an analysis the workflow lacks may you change ${WF_DIR}/.
- Always tee the simulation stdout into <outdir>/run.log (the advect timings and area drifts live only there).`,
    author: () => `Steps: read ${WF_DIR}/README.md; build; run the simulation(s) the task needs (${A.outdir ? `use the output directory \`${A.outdir}\`` : 'pick an output directory; create a fresh run if none fits'}); run verify.py on it; read report.md, metrics.json and the PNGs; then compose the analysis as markdown (returned in analysis_markdown, not written to a file) answering the task point by point, citing the file and field behind each statement. Note anything surprising (where the max error sits relative to the interface, stencil check results, cell- vs face-centred quantities at the examined (i,j)).`,
    schema: {
      type: 'object',
      properties: {
        outdir: { type: 'string' },
        commands: { type: 'array', items: { type: 'string' } },
        artifacts: { type: 'array', items: { type: 'string' } },
        analysis_markdown: { type: 'string', description: 'the full write-up answering the task, markdown' },
        claims: {
          type: 'array',
          items: { type: 'object', properties: { claim: { type: 'string' }, evidence: { type: 'string' } }, required: ['claim', 'evidence'] },
        },
        open_questions: { type: 'array', items: { type: 'string' } },
      },
      required: ['outdir', 'commands', 'artifacts', 'analysis_markdown', 'claims'],
    },
    review: (d) => `Checks:
1. Every part of the task specification is answered in the author's analysis_markdown (shown above); a missing or partial write-up is a major issue.
2. The artifacts exist and are current, and <outdir>/run.log exists; re-run \`python3 ${WF_DIR}/verify.py ${d.outdir} --dest /tmp/verify_review\` and compare (writing to /tmp is allowed).
3. Re-derive each claim from its evidence file (metrics.json, report.md, history.csv, run.log). Flag any number that does not match, any mislabelled norm (L1 / L2 / Linf, RMS vs sum), any cell- vs face-centre confusion.
4. Stencil FAILs, or a history cross-check reported as INCONSISTENT, must be explained in the analysis.
5. Open the PNGs and confirm they show what the analysis says.
Do not edit repo files; scratch output under /tmp is fine.`,
    result: (d) => ({ outdir: d.outdir, analysis_markdown: d.analysis_markdown, artifacts: d.artifacts, claims: d.claims }),
  },

  evaluator: {
    rules: `
Rules (evaluator):
- You may edit only: the target source file(s) named by the task under src/ (EVOLVE-BLOCK markers, plus the fixed harness outside the block: LowPrec declarations in the matching header, fp64<->low-precision conversion, a precision dispatch if needed), dev/workflows/evaluator/experiments/<name>/, and a new params/<name>_eval.ini if the experiment needs its own config. Never edit tests/, Makefile, or other experiments.
- Insert markers only with \`python3 dev/workflows/evaluator/embed_evolve_block.py <file> --functions ...\`; never hand-write them.
- The fp64 reference path and its public API must stay bit-for-bit unchanged: \`make check\` must stay green.
- An experiment is dev/workflows/evaluator/experiments/<name>/{experiment.yaml, evaluator.py}; follow dev/workflows/evaluator/README.md and use experiments/advect-mxp/ as the template. The evaluator must audit the block (no 64-bit FP tokens in the source, no *sd/*pd arithmetic in the *LowPrec functions of the object file), run the simulation once, and score from history.csv.`,
    author: () => `Steps: read dev/workflows/evaluator/README.md and experiments/advect-mxp/; decide the experiment name and target functions from the task; embed or confirm the EVOLVE-BLOCK; create or update experiment.yaml and evaluator.py; run \`make\`, \`make check\`, \`python3 dev/workflows/alphaevolve/run.py check <exp>\` and \`... baseline <exp>\`. Then do the audit canary on a scratch copy, never in the repo: \`rsync -a --exclude .git --exclude build --exclude 'out*' ./ /tmp/canary_<name>/\`; in the copy, (a) put a \`double\` variable inside the block and run the copy's evaluator from /tmp/canary_<name> (it must fail with FORBIDDEN_TOKEN), (b) restore, then add an unsuffixed literal that promotes to double arithmetic (e.g. \`x * 0.1\` on a float) and run again (it must fail with FORBIDDEN_FP64_INSTRUCTION); then delete /tmp/canary_<name>.`,
    schema: {
      type: 'object',
      properties: {
        experiment_dir: { type: 'string' },
        target_files: { type: 'array', items: { type: 'string' } },
        evolved_functions: { type: 'array', items: { type: 'string' } },
        files_changed: { type: 'array', items: { type: 'string' } },
        commands: { type: 'array', items: { type: 'string' } },
        make_check: { type: 'string', description: 'last line of make check output' },
        baseline_scores: {
          type: 'array',
          items: { type: 'object', properties: { metric: { type: 'string' }, score: { type: ['number', 'null'] } }, required: ['metric', 'score'] },
        },
        canary: {
          type: 'object',
          properties: {
            source_token_rejected: { type: 'boolean' },
            fp64_instruction_rejected: { type: 'boolean' },
            details: { type: 'string' },
          },
          required: ['source_token_rejected', 'fp64_instruction_rejected', 'details'],
        },
        notes: { type: 'string' },
      },
      required: ['experiment_dir', 'target_files', 'evolved_functions', 'files_changed', 'commands', 'make_check', 'baseline_scores', 'canary'],
    },
    review: (d) => `Checks:
1. \`git status --short\` and \`git diff\`: only files the evaluator rules allow were touched; the fp64 reference path is unchanged.
2. \`python3 dev/workflows/evaluator/embed_evolve_block.py <file> --list\` for each of ${JSON.stringify(d.target_files)}: the block wraps exactly ${JSON.stringify(d.evolved_functions)}, and the code outside the block is harness only.
3. \`make\` then \`make check\`: must end with 0 failed.
4. \`python3 dev/workflows/alphaevolve/run.py check ${d.experiment_dir}\` and \`... baseline ${d.experiment_dir}\`: both succeed, and the scores match the author's ${JSON.stringify(d.baseline_scores)} (timing-based scores may differ by a few percent).
5. experiment.yaml: the problem_description states the rules the evaluator really enforces; primary_metric is one of the evaluator's metrics; seed_files lists exactly the files with blocks.
6. Repeat at least one audit canary yourself on a fresh scratch copy (/tmp/canary_review, deleted afterwards) and confirm it is rejected.
Only scratch copies may be edited; do not change repo files.`,
    result: (d) => ({ experiment_dir: d.experiment_dir, files_changed: d.files_changed, baseline_scores: d.baseline_scores, canary: d.canary, make_check: d.make_check }),
  },
}

if (WORKFLOW === 'alphaevolve') {
  throw new Error('myloop: alphaevolve is triggered independently (dev/workflows/alphaevolve/run.py)')
}
const P = PROFILES[WORKFLOW]
if (!P) throw new Error(`myloop: unknown workflow '${WORKFLOW}' (supported: ${Object.keys(PROFILES).join(', ')})`)

// ---- author-reviewer loop -----------------------------------------------------

phase('Author')
let draft = null
let review = null
const history = []

for (let round = 1; round <= MAX_ROUNDS; round++) {
  const feedback = review
    ? `\n\nThe reviewer did NOT approve round ${round - 1}. Address every issue below, then redo whatever is affected:\n` +
      review.issues.map((i, k) => `${k + 1}. [${i.severity}] ${i.issue}\n   fix: ${i.fix}`).join('\n') +
      `\n\nYour previous result:\n${JSON.stringify(draft, null, 2)}`
    : ''

  draft = await agent(
    `You are the AUTHOR in an author-reviewer loop, using the \`${WORKFLOW}\` workflow (${WF_DIR}/).

TASK SPECIFICATION:
${A.task}
${COMMON}
${P.rules}

${P.author()}${feedback}

Return the structured result.`,
    { label: `author r${round}`, phase: 'Author', schema: P.schema },
  )
  if (!draft) throw new Error(`myloop: author agent failed in round ${round}`)

  review = await agent(
    `You are the REVIEWER in an author-reviewer loop for the \`${WORKFLOW}\` workflow (${WF_DIR}/). Be skeptical: approve only if the task is fully and correctly done. Check things yourself; do not take the author's word for anything.

TASK SPECIFICATION:
${A.task}
${COMMON}
${P.rules}

The author reports:
${JSON.stringify(draft, null, 2)}

${P.review(draft)}

Return approved=true only with zero blocker/major issues; list minor issues either way, and record every check you ran in checks_run.`,
    { label: `review r${round}`, phase: 'Review', schema: REVIEW_SCHEMA },
  )
  if (!review) throw new Error(`myloop: reviewer agent failed in round ${round}`)

  history.push({ round, approved: review.approved, issues: review.issues.length, summary: review.summary })
  log(`round ${round}: ${review.approved ? 'APPROVED' : 'changes requested'} (${review.issues.length} issue(s))`)
  if (review.approved) break
}

if (!review.approved) log(`not approved after ${MAX_ROUNDS} round(s); returning the last draft with open issues`)

return {
  workflow: WORKFLOW,
  approved: review.approved,
  rounds: history,
  ...P.result(draft),
  checks_run: review.checks_run,
  open_issues: review.approved ? review.issues.filter(i => i.severity === 'minor') : review.issues,
  review_summary: review.summary,
}
