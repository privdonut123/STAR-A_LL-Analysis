# Pi0 (Pion) Finder for EEMC — a_ll_analysis

## Overview

This implementation reconstructs π0 candidates from photon pairs detected in the EEMC (End-Cap Electromagnetic Calorimeter). It runs on MuDst files using the STAR framework and is designed to be submitted as Condor batch jobs via `MakeJob_runAnalysis.pl`.

---

## Directory Contents

| File / Directory | Purpose |
|---|---|
| `runAnalysis.C` | Top-level ROOT macro — sets up the full maker chain and runs the event loop |
| `MakeJob_runAnalysis.pl` | Perl script that creates and submits Condor batch jobs |
| `validate_mudsts.C` | ROOT macro to pre-screen MuDst files for EEMC data before analysis |
| `merge_mudst_lists.pl` | Perl script that merges per-run `*_good_mudsts.list` and `*_failed_mudsts.list` files into single combined lists |
| `StRoot/StPi0FinderForEEmc/` | Custom STAR maker that performs the π0 reconstruction |

---

## Maker Files — `StRoot/StPi0FinderForEEmc/`

These files define the custom STAR maker that plugs into the StChain event loop.

### `StPi0FinderForEEmc.h` / `StPi0FinderForEEmc.cxx`

The main analysis maker, inheriting from `StMaker`. It is added to the chain in `runAnalysis.C` and called once per event.

**Responsibilities:**
- Retrieves calibrated EEMC energy deposits from `StEEmcEnergyMaker_t`
- Retrieves SMD-clustered 3D hit points from `StEEmcHitMakerSimple_t`
- Finds photon candidates via `FindEEmcPhotons()`
- Pairs all photon candidates and computes invariant mass, asymmetry, and kinematics
- Applies kinematic cuts and fills output histograms and a TTree

**Configuration setters:**

```cpp
pi0Finder->SetHistogramFile("output_qa.root");    // QA histogram output file
pi0Finder->SetTreeFile("output_candidates.root"); // Candidate tree output file
pi0Finder->SetPhotonEnergyMin(0.05);              // Min photon energy [GeV]
pi0Finder->SetPi0EnergyMin(2.0);                  // Min pi0 total energy [GeV]
pi0Finder->SetPi0MassMax(0.2);                    // Max invariant mass [GeV]
pi0Finder->SetAsymmetryMax(0.8);                  // Max energy asymmetry zgg
pi0Finder->SetEnergyMaker(energyMaker);           // Link to StEEmcEnergyMaker_t
pi0Finder->SetHitMaker(hitMaker);                 // Link to StEEmcHitMakerSimple_t
pi0Finder->SetDebugLevel(4);                      // Verbosity (0=quiet, higher=more)
```

### `EEmcPhotonUtils.h`

Standalone utility header (no `.cxx` needed). Defines:

- **`EEmcPhoton` struct** — photon candidate container holding energy, 3D position, sector, tower count, and χ²; provides `Get4Vector()`, `Eta()`, `Phi()`, `Pt()` helpers.
- **`EEmcPi0Kinematics` class** — static methods for invariant mass (4-vector and parametric), energy asymmetry (zgg), and opening angle calculations.

---

## `runAnalysis.C` — Analysis Macro

Sets up the full STAR maker chain and runs the event loop. This is what gets executed on each Condor worker node.

### Signature

```cpp
void runAnalysis(
    Int_t   nFiles       = 1,
    TString InputFileList = "<default MuDst path>",
    Int_t   nEvents      = 10000,
    TString outputPrefix = "pi0_analysis",
    Int_t   debug        = 4
)
```

| Argument | Description |
|---|---|
| `nFiles` | Number of files `StMuDstMaker` reads from the list |
| `InputFileList` | Path to a single `.MuDst.root` file or a text file listing MuDst files (one per line) |
| `nEvents` | Maximum events to process |
| `outputPrefix` | Prefix for output files — produces `<prefix>_qa.root` and `<prefix>_candidates.root` |
| `debug` | Debug verbosity passed to `StPi0FinderForEEmc` |

### Usage

```bash
# Single file, interactive
root4star -b -q 'runAnalysis.C(1, "st_physics_....MuDst.root", 1000, "test_out")'

# From a file list, 10000 events
root4star -b -q 'runAnalysis.C(5, "input.list", 10000, "pi0_run16")'
```

### Maker Chain

`runAnalysis.C` assembles the following makers in order:

| Maker | Role |
|---|---|
| `StMuDstMaker` | Reads MuDst input files; sets status flags to load Event, PrimaryVertices, EmcAll branches |
| `StMuDst2StEventMaker` | Converts MuDst data to `StEvent` format required by downstream makers |
| `St_db_Maker` | Connects to the STAR database for run-by-run calibration constants (TPC/SVT/FGT blacklisted) |
| `StEEmcDbMaker` | Loads EEMC-specific calibration constants from the database |
| `StEEmcA2EMaker` | Converts raw EEMC ADC values to calibrated energies; applies 3σ thresholds for tower, pre/post-shower, and SMD layers |
| `StEEmcEnergyMaker_t` | Applies tower (1.0 ADC) and strip (0.001 ADC) thresholds and organizes calibrated energies |
| `StEEmcStripClusterFinderIU_t` | Finds clusters in SMD U/V strip layers (IU algorithm) |
| `StEEmcPointFinderIU_t` | Reconstructs 3D photon hit points from U/V strip cluster intersections |
| `StEEmcEnergyApportionerIU_t` | Distributes tower energy among overlapping SMD points |
| `StEEmcHitMakerSimple_t` | Wraps the cluster/point finders above; SMD strip clustering enabled, tower/pre/post-shower clustering disabled |
| `StPi0FinderForEEmc` | **Analysis maker** — finds photon candidates, pairs them, applies cuts, fills output |

### Output Files

**`<prefix>_qa.root`** — QA histograms:

| Histogram | Content |
|---|---|
| `h1_photon_energy` | Photon energy spectrum |
| `h1_photon_eta` | Photon η distribution |
| `h1_photon_phi` | Photon φ distribution |
| `h2_photon_energy_vs_eta` | 2D: photon energy vs η |
| `h2_photon_position` | 2D: photon hit positions |
| `h2_detector_hitmap` | 2D: all EEMC tower hit positions |
| `h1_detector_phi` | φ distribution of detector hits |
| `h1_pi0_mass` | π0 invariant mass spectrum (signal peak ~0.135 GeV) |
| `h1_pi0_energy` | π0 total energy |
| `h1_pi0_pt` | π0 transverse momentum |
| `h1_pi0_eta` | π0 η distribution |
| `h1_pi0_opening_angle` | Opening angle between photon pairs |
| `h1_pi0_asymmetry` | Energy asymmetry zgg |
| `h2_pi0_mass_vs_energy` | 2D: invariant mass vs total energy |
| `h2_pi0_position` | 2D: π0 spatial distribution |

**`<prefix>_candidates.root`** — TTree `pi0tree` with branches:

```
event_number, run_number
pi0_energy, pi0_mass, pi0_pt, pi0_eta, pi0_phi
opening_angle, asymmetry
pi0_pos_x, pi0_pos_y, pi0_pos_z
ph1_energy, ph1_eta, ph1_phi, ph1_pos_x, ph1_pos_y, ph1_pos_z
ph2_energy, ph2_eta, ph2_phi, ph2_pos_x, ph2_pos_y, ph2_pos_z
```

---

## `validate_mudsts.C` — MuDst Pre-screening Macro

Validates a list of MuDst files before analysis by checking that each file can be opened by ROOT and contains a `MuDst` TTree with non-empty EEMC branches (`EEmcPrs`, `EEmcSmdu`, `EEmcSmdv`).

### Signature

```cpp
void validate_mudsts(
    const char* inputPath       = "input_files.list",
    const char* outputFile      = "good_mudsts.list",
    bool        isDirectory     = false,
    bool        useGetFileList  = false,
    const char* failedOutputFile = "failed_mudsts.list"
)
```

| Argument | Description |
|---|---|
| `inputPath` | Path to a text file listing MuDst files, a directory, or a single `.MuDst.root` file |
| `outputFile` | Output list of files that passed validation (have EEMC data) |
| `isDirectory` | Set `true` to scan `inputPath` as a directory |
| `useGetFileList` | Deprecated — do not use; `MakeJob_runAnalysis.pl` handles `get_file_list.pl` directly |
| `failedOutputFile` | Output list of files that could not be opened or had no `MuDst` tree |

The failed list annotates each entry with a reason (`# cannot open` or `# no MuDst tree`). Files that open successfully but have no EEMC data are reported as "skipped" in the summary but are not written to the failed list — they are valid ROOT files, just not useful for this analysis.

### Usage

```bash
# Validate a text file list
root4star -b -q 'validate_mudsts.C("input.list", "good.list", false, false, "failed.list")'

# Validate a directory
root4star -b -q 'validate_mudsts.C("/path/to/mudsts", "good.list", true)'
```

When invoked via `MakeJob_runAnalysis.pl -V`, the macro is called automatically on each worker node before `runAnalysis.C` runs.

---

## `merge_mudst_lists.pl` — Merge Validation Output Lists

After a validation job completes, each Condor worker writes its own `*_good_mudsts.list` and `*_failed_mudsts.list` into the output directory. This script collects all of them, deduplicates, and writes two merged files ready for use as input to `MakeJob_runAnalysis.pl`.

### Synopsis

```
merge_mudst_lists.pl -d <output_dir> [-g <good.list>] [-f <failed.list>] [-v <level>] [-h]
```

### Options

| Option | Argument | Default | Description |
|---|---|---|---|
| `-d` | path | — | Directory containing `*_good_mudsts.list` and `*_failed_mudsts.list` files (required) |
| `-g` | path | `<dir>/merged_good_mudsts.list` | Output path for the merged good-files list |
| `-f` | path | `<dir>/merged_failed_mudsts.list` | Output path for the merged failed-files list |
| `-v` | integer | `1` | Verbosity: `1`=summary + progress ticker; `2`=per-file counts + DEBUG lines; `3`=per-line trace |
| `-h` | flag | — | Print help and exit |

### Usage

```bash
# Merge into the same directory as the source lists (default output paths)
./merge_mudst_lists.pl -d /path/to/validation/Output

# Write merged files to a custom location
./merge_mudst_lists.pl \
  -d /path/to/validation/Output \
  -g /path/to/merged_good_mudsts.list \
  -f /path/to/merged_failed_mudsts.list

# Verbose: show per-file entry counts
./merge_mudst_lists.pl -d /path/to/validation/Output -v 2

# Trace every line processed (for debugging a specific run)
./merge_mudst_lists.pl -d /path/to/validation/Output -v 3
```

### Output

Two files are written:

- **`merged_good_mudsts.list`** — all unique MuDst paths that passed validation (have EEMC data). One path per line. Pass directly to `MakeJob_runAnalysis.pl -d`.
- **`merged_failed_mudsts.list`** — all unique MuDst paths that failed validation, each annotated with the reason (`# cannot open` or `# no MuDst tree`). Failed entries are deduplicated by filepath so the same file appearing in multiple per-run lists is counted once regardless of the failure reason.

Both files include a comment header with the source directory, number of source files, and total entry count.

### Typical workflow

```bash
# 1. Submit validation jobs
./MakeJob_runAnalysis.pl -V -l "..." -o /path/to/validation

# 2. Wait for jobs to finish, then merge the results
./merge_mudst_lists.pl -d /path/to/validation/Output

# 3. Use the merged good list as input for the analysis jobs
./MakeJob_runAnalysis.pl -d /path/to/validation/Output/merged_good_mudsts.list -o /path/to/analysis
```

---

## `MakeJob_runAnalysis.pl` — Condor Job Submission

Creates and submits HTCondor batch jobs that run `runAnalysis.C` over MuDst files. Input files are grouped by run number; one Condor job is created per run group.

### Synopsis

```
MakeJob_runAnalysis.pl [options]
```

### Options

| Option | Argument | Default | Description |
|---|---|---|---|
| `-a` | path | `$PWD` | Analysis directory containing `runAnalysis.C` |
| `-d` | path | — | MuDst file, text list, or directory (mutually exclusive with `-l`) |
| `-l` | conditions | — | Conditions string passed to `get_file_list.pl` (mutually exclusive with `-d`) |
| `-o` | path | `$PWD` | Output directory for job folders |
| `-n` | integer | `10000` | Number of events to process per file |
| `-N` | integer | all (5 in test) | Number of input files to process; `-N -1` means unlimited |
| `--limit` | integer | 1000 (10 in test) | Limit passed to `get_file_list.pl` when using `-l` |
| `-u` | string | random UUID | Custom name for the job directory |
| `-p` | string | — | Message recorded in the job summary file |
| `-b` | flag | off | Batch mode: subdivide each run's file list into smaller batches (one job per batch) |
| `-B` | integer | `10` | Files per batch when batch mode (`-b`) is on |
| `-t` | flag | off | Test mode: 5 files, 100 events/file |
| `-V` | flag | off | Validate MuDst files on the worker node with `validate_mudsts.C` before running analysis |
| `-w` | flag | off | Disable saving job stdout |
| `-e` | flag | off | Disable saving job stderr |
| `-f` | flag | off | Force overwrite of existing job directory without prompting |
| `-v` | integer | `1` | Verbosity level |
| `-h` | flag | — | Print help and exit |

### Examples

```bash
# Test run: 5 files from a directory, 100 events each
./MakeJob_runAnalysis.pl -t -d /path/to/mudst_dir -o /path/to/output

# Production run from get_file_list.pl query, 10000 events/file, with MuDst validation
./MakeJob_runAnalysis.pl \
  -l "production=P16id,filetype=daq_reco_MuDst,trgsetupname=production_pp200long2_2015,filename~st_physics,storage=local" \
  -o /star/u/bmagh001/output \
  -n 10000 \
  -V

# Batch mode: split each run into jobs of 15 files each
./MakeJob_runAnalysis.pl -d /path/to/mudst_dir -o /path/to/output -b -B 15

# Process only 20 files from a list, save to a named directory
./MakeJob_runAnalysis.pl -d input.list -N 20 -u my_run16_test -o /star/u/bmagh001/output

# Force re-run of an existing job directory without prompting
./MakeJob_runAnalysis.pl -d input.list -u my_run16_test -o /star/u/bmagh001/output -f
```

### Job Structure

For each run number group, the script writes:
- `<outdir>/<jobid>/condor/lists/Files_<run>.list` — file list for that run (no batch mode)
- `<outdir>/<jobid>/condor/lists/Files_<run>_001.list`, `Files_<run>_002.list`, … — per-batch file lists (batch mode)
- `<outdir>/<jobid>/condor/RunAnalysis.csh` — worker-node shell script
- `<outdir>/<jobid>/condor/runAnalysis.C` — copy of the analysis macro
- `<outdir>/<jobid>/condor/validate_mudsts.C` — copy of the validation macro (if `-V` used)
- `<outdir>/<jobid>/condor/.sl73_x8664_gcc485/` — compiled libraries (if present)
- `<outdir>/<jobid>/Summary_<UUID>.list` — summary of all input files and job parameters

Output files produced on the worker node follow the same naming convention as the input lists:
- No batch mode: `<jobid>_<run>_qa.root` and `<jobid>_<run>_candidates.root`
- Batch mode: `<jobid>_<run>_001_qa.root`, `<jobid>_<run>_001_candidates.root`, etc.

`RunAnalysis.csh` receives four positional arguments from Condor:
1. Number of files in the run group (or batch)
2. Number of events per file
3. Path to the file list
4. Output file prefix

If `-V` is set, the shell script runs `validate_mudsts.C` on the file list first and feeds the resulting good-files list to `runAnalysis.C`.

---

## Compilation

> **SL7 container required.** The STAR framework and its libraries are built for Scientific Linux 7 (`sl73_x8664_gcc485`). Compilation and local macro execution must be done inside the SL7 Singularity container. On RCF/SDCC nodes running Alma 9, enter the container first:
> ```bash
> singularity exec -e -B /direct -B /star -B /afs -B /gpfs -B /sdcc/lustre02 /cvmfs/star.sdcc.bnl.gov/containers/rhic_sl7.sif csh
> ```

The `StPi0FinderForEEmc` maker must be compiled before running. Inside the SL7 container:

```bash
cd /path/to/a_ll_analysis
stardev
cons
```

This produces `./.$STAR_HOST_SYS/lib/libStPi0FinderForEEmc.so`, which `runAnalysis.C` loads at runtime from `./.sl73_x8664_gcc485/lib/`.

**Required libraries loaded by `runAnalysis.C`:**
`StDetectorDbMaker`, `StDbUtilities`, `StDbBroker`, `St_db_Maker`, `StEEmcUtil`, `StEEmcDbMaker`, `StEEmcA2EMaker`, `StSpinDbMaker`, `StTriggerUtilities`, `StEEmcPoolEEmcTreeContainers`, `StEEmcTreeMaker`, `StEEmcHitMaker`, `StEEmcPool`

---

## Physics Algorithm

### Pion Reconstruction

1. **Photon finding** — `FindEEmcPhotons()` retrieves `EEmcParticleCandidate_t` objects from `StEEmcHitMakerSimple_t` (3D points reconstructed from SMD U/V strip cluster intersections).
2. **Pairing** — all unique photon pairs are formed.
3. **Kinematic cuts** — pairs must satisfy:
   - Both photon energies ≥ `mPhotonEnergyMin` (default 0.05 GeV)
   - Total pair energy ≥ `mPi0EnergyMin` (default 2.0 GeV)
   - Invariant mass ≤ `mPi0MassMax` (default 0.2 GeV)
   - Energy asymmetry |zgg| ≤ `mAsymmetryMax` (default 0.8)
4. **Output** — passing candidates are stored in the TTree and QA histograms are filled.

### Invariant Mass

```
M² = (E₁ + E₂)² − |p⃗₁ + p⃗₂|²
```

---

## Debugging

```bash
# Increase debug level in runAnalysis.C
pi0Finder->SetDebugLevel(4);   # higher = more verbose

# Inspect output interactively
root my_pi0_analysis_qa.root
root [0] h1_pi0_mass->Draw()
root [1] h2_detector_hitmap->Draw("colz")

root my_pi0_analysis_candidates.root
root [0] pi0tree->Draw("pi0_mass")
root [1] pi0tree->Scan("event_number:pi0_mass:pi0_energy")
```

---

## References

- EEMC Utilities: STAR framework `StRoot/StEEmcUtil/`
- Related: `BrightSTAR/RunEEmcNanoDstMaker.cxx`, `BrightSTAR/ReadEEmcNanoTree.C`
- STAR MuDst documentation: `StRoot/StMuDSTMaker/`
