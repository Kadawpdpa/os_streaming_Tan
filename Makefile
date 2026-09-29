.RECIPEPREFIX = >
.PHONY: run clean
run clean:
> $(MAKE) --no-print-directory -C CODE_PJ $@
