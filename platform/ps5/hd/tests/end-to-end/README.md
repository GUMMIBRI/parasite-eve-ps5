# Synthetic cross-module lifetime check

No production target changes. Use caller-selected host SDL3 prefix and C compiler:

```sh
cmake -S platform/ps5/hd/tests/end-to-end -B build/hd-e2e \
  -DPE_HD_CHECKS=ON -DPE_PS5_HD_STAGING=ON -DSDL3_DIR=/selected/SDL3/lib/cmake/SDL3
cmake --build build/hd-e2e --target check-end-to-end
ctest --test-dir build/hd-e2e -R '^hd-end-to-end$' --output-on-failure
```

Use a short writable build path on Windows. Zig requires explicit `cc` compiler
argument and suitable archive tools; put its local/global cache in writable paths.
The executable creates one temporary directory beneath its working directory,
writes only a generated synthetic PNG, and removes both on success. Test assertion
failure deliberately preserves the fixture as evidence. No assets or console needed.

The key comes from the synchronized synthetic VRAM snapshot. TSV ingestion copies
its records before input destruction. pe_hd_pipeline_adopt consumes that fresh
manifest index directly, setting the caller pointer NULL only on success. There
is no duplicate index or record reconstruction. Index budgets remain those chosen
by manifest construction; adoption takes only the cache budget.

NULL arguments and invalid cache budgets preserve both caller index and output.
The fixture pins/reads the retained index after failed creation to prove usability,
then destroys it and constructs a fresh index for adoption. Reusing the consumed
NULL pointer fails without altering the live pipeline. Existing check-pipeline
continues testing the records-based constructor.

A held decoded lease survives generation cancellation, lookup destruction and
pipeline BUSY close. An independent direct pack pin (acquired on a worker, without
accessing the adopted index) keeps pack close BUSY through final pipeline drain.
Releasing it permits pack close, proving zero outstanding provider pins. Exactly
three provider reads occur, all off the staging thread. This is a trusted immutable
synthetic root, not a symlink confinement or rendering compatibility test.

Exclusive fresh index ownership is a caller precondition: opaque index APIs do
not expose alias/past-use checks. After transfer no direct index calls are legal.
Provider lifetime extends through successful pipeline close. A decoded lease alone
does not retain encoded bytes; providers must not infer pin lifetime from it.
No private assets, console execution or third-party implementation added.

