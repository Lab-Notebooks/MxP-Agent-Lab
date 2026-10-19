# Define the subdirectories
# The order in this list is mostly for the 'clean' target loop
SUBDIRS = common example_1 example_2 example_3 example_4

# The final target to build is the last one in the dependency chain
all: example_1 example_2 example_3 example_4

# Define the dependencies between subdirectories
example_1: common
example_3: common

# Rule for building a single subdirectory
$(SUBDIRS):
	@echo "Making in $(@)"
	@$(MAKE) -C $(@)

clean:
	@echo "Cleaning in $(SUBDIRS)"
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

# Phony targets
.PHONY: all clean $(SUBDIRS)
