# RED2 v0.35 — public repository overview

Experimental prime-sieving implementation with blocked-cofactor storage and product-calendar scheduling. This repository contains the C source, headers, shell runners, Python evidence-reproduction scripts, documentation, and supplied benchmark evidence from the original archive.

## Start here

- [README.md](README.md): original usage instructions and commands.
- [DESIGN.md](DESIGN.md): algorithm and correctness rationale.
- [LOCAL_RESULTS.md](LOCAL_RESULTS.md): recorded benchmark results and limitations.
- [evidence/README.md](evidence/README.md): evidence descriptions.

## Evidence boundaries

The benchmark records are **supplied historical results**, not measurements rerun during this repackaging. The documented local measurements compare RED2 v0.35 with v0.34 on a specific Linux host; they do not establish a universal win over primesieve. Read the original limitations before quoting performance.

## Building and testing

Follow the original README for MSYS2 UCRT64 and build/validation scripts. Generated Windows executables have been excluded; compile them locally. The source package has not been rebuilt or tested as part of GitHub preparation.

## Publication checks

Review ownership, third-party license obligations, any private information in logs or metadata, and permission to publish before making this repository public. No open-source license has been granted by this packaging process.
