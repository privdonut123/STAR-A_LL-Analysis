#!/usr/bin/perl

use strict;
use warnings;
use lib '/star/u/dkap7827/Tools2/Tools/PerlScripts';
use CondorJobWriter;
use Cwd qw(abs_path);
use Getopt::Long qw(GetOptions);
use Getopt::Long qw(HelpMessage);
use Pod::Usage;

Getopt::Long::Configure('no_ignore_case');

=pod

=head1 NAME

MakeJob_runAnalysis - Simplified script for submitting condor jobs for runAnalysis.C

=head1 SYNOPSIS

MakeJob_runAnalysis.pl [-a I<path{.}>] (-d I<path> | -l I<getfilelist_args>) [-o I<path{.}>] [-w] [-u I<name>] [-n I<nevents>] [-e] [-p I<"Message">] [-t] [-V] [-v I<level{1}>] [-h]

=over 4

=item B<-a> analysis code directory (where runAnalysis.C is located)

=item B<-d> data file or directory with MuDst files (alternative to -l)

=item B<-l> custom conditions for get_file_list.pl query (uses standard production conditions if not specified)

=item B<-o> directory to store output (default current directory)

=item B<-u> custom name for job directory (default is random UUID)

=item B<-n> number of events to process per file (default 10000)

=item B<-e> disable writing job stderr (no StdErr saved)

=item B<-L> limit number of files returned by get_file_list.pl (default 1000, 10 in test mode)

=item B<-N> number of files to process (default: all; in test mode, default is 5)

=item B<-p> message to include in summary file

=item B<-b> enable batch mode (subdivides each run's file list into smaller batches)

=item B<-B> batch size when batch mode is on (default 10 files per batch)

=item B<-t> test mode (processes only 5 files)

=item B<-V> validate MuDst files with ValidateMudsts.pl before job creation

=item B<-v> verbosity level (default 1)

=item B<-h> print this help and exit

=back

=head1 DESCRIPTION

This script creates and submits condor jobs for running the runAnalysis.C macro on MuDst files.
It simplifies MakeJob.pl by supporting only the runAnalysis workflow.

=head1 VERSION

1.0

=cut

my $LOC = $ENV{'PWD'};
my $ANADIR = $LOC;
my $DATA = "";
my $GETFILELIST_ARGS = "";
my @DATAFILES;
my $OUTDIR = $LOC;
my $VERBOSE = 1;
my $NEVENTS = 10000;
my $UUID;
my $TEST;
my $MSG = "";
my $FORCE;
my $VALIDATE_MUDSTS = 0;
my $NOWRITESTDOUT = 0;
my $NOWRITESTDERR = 0;
my $LIMIT; # limit for get_file_list.pl
my $NFILES; # number of files to process
my $GETFILELIST_CMD = "";
my $BATCH = 0;
my $BATCHSIZE = 10;

my $thiscommand = "$0";
foreach my $arg (@ARGV){ $thiscommand = "$thiscommand" . " $arg"; }

GetOptions(
    'ana|a=s'     => \$ANADIR,
    'data|d=s'    => \$DATA,
    'list|l=s'    => \$GETFILELIST_ARGS,
    'out|o=s'     => \$OUTDIR,
    'nowritestdout|w' => \$NOWRITESTDOUT,
    'msg|p=s'     => \$MSG,
    'nevent|n=i'  => \$NEVENTS,
    'nowritestderr|e' => \$NOWRITESTDERR,
    'limit=i'     => \$LIMIT,
    'nfiles|N=i'   => \$NFILES,
    'batch|b'      => \$BATCH,
    'batchsize|B=i' => \$BATCHSIZE,
    'test|t'      => \$TEST,
    'validate|V'  => \$VALIDATE_MUDSTS,
    'uuid|u=s'    => \$UUID,
    'verbose|v=i' => \$VERBOSE,
    'force|f'     => \$FORCE,
    'help|h'      => sub { HelpMessage(0) }
    ) or HelpMessage(1);

# Validate analysis directory
if( !(-d "$ANADIR") ){ 
    print "ERROR: Analysis directory '$ANADIR' does not exist\n";
    HelpMessage(0);
}
else{
    my $char = chop $ANADIR;
    while( $char eq "/" ){$char = chop $ANADIR;}
    $ANADIR = $ANADIR.$char;
}
if( $VERBOSE>=1 ){ print "ANADIR=$ANADIR\n"; }

# Detect Alma 9 nodes and set environment for singularity container
my $ALMA9JOB = 0; #Boolean for indicating jobs submitted through Alma 9
if( $ENV{'HOST'} =~ /starsub\d+/ ){
    $ALMA9JOB = 1;
    if( $VERBOSE>=1 ){ print "Alma 9 node detected\n"; }
}
#@[July 23, 2025] > Hack to fix the star environment to use the rcas compiled library because STAR doesn't compile on Alma 9 which has a different STAR_HOST_SYS
if( $ALMA9JOB ){ $ENV{'STAR_HOST_SYS'} = "sl73_x8664_gcc485"; }

# If test mode requested, set a small number of events per file
if( $TEST ){
    $NEVENTS = 100;
    if( $VERBOSE>=1 ){ print "Test mode: setting events per file to $NEVENTS\n"; }
}

# Check for runAnalysis.C
if( !(-f "$ANADIR/runAnalysis.C") ){
    print "ERROR: runAnalysis.C not found in $ANADIR\n";
    HelpMessage(0);
}

# Validate data input
if( $DATA eq "" && $GETFILELIST_ARGS eq "" ){
    print "ERROR: Must specify either -d (directory/file) or -l (getfilelist arguments)\n";
    HelpMessage(0);
}

if( $DATA ne "" && $GETFILELIST_ARGS ne "" ){
    print "ERROR: Cannot specify both -d and -l options\n";
    HelpMessage(0);
}

# Get files either from directory or from get_file_list.pl
if( $GETFILELIST_ARGS ne "" ){
    if( $VERBOSE>=1 ){ print "Querying get_file_list.pl with condition: $GETFILELIST_ARGS\n"; }
    
    my $cond = $GETFILELIST_ARGS;
    # If user provided nothing after -l (shouldn't happen), fall back to default below
    if( $cond eq "" ){
        $cond = "production=P16id,filetype=daq_reco_MuDst,trgsetupname=production_pp200long2_2015,filename~st_physics,storage=local";
    }
    # Determine limit: if user didn't provide -limit, set default (1000), or 10 in test mode
    # If user requested unlimited files via -N -1, pass -limit -1 to get_file_list.pl
    if( defined $NFILES && $NFILES < 0 ){
        $LIMIT = -1;
    }
    # Determine limit: if user didn't provide -limit, set default (1000), or 10 in test mode
    elsif( !defined $LIMIT ){
        $LIMIT = $TEST ? 10 : 1000;
    }
    $GETFILELIST_CMD = "get_file_list.pl -keys 'path,filename' -delim '/' -limit $LIMIT -cond '$cond'";
    if( $VERBOSE>=1 ){ print "Running: $GETFILELIST_CMD\n"; }
    my $filelist_output = `$GETFILELIST_CMD`;
    if( $? != 0 ){
        print "ERROR: get_file_list.pl command failed\n";
        print "Command: $GETFILELIST_CMD\n";
        print "Output: $filelist_output\n";
        exit(1);
    }
    
    my @lines = split(/\n/, $filelist_output);
    foreach my $line (@lines){
        chomp($line);
        next if $line eq "";
        next if $line =~ /^#/;
        # get_file_list.pl with -keys 'path,filename' prints two columns: path filename
        my @cols = split(/\s+/, $line);
        my $fullpath;
        if( scalar(@cols) == 1 ){
            $fullpath = $cols[0];
        }
        else{
            my ($path, $fname) = ($cols[0], $cols[1]);
            $path =~ s/\/+\z//; # remove trailing slash if any (no-op if none)
            $path =~ s/\/$//;
            $fullpath = "$path/$fname";
        }
        
        # If file is on distributed disk (DD), prepend XRootD prefix
        if( $fullpath =~ m|^/home/starlib/| ){
            $fullpath = "root://xrdstar.rcf.bnl.gov:1095/" . $fullpath;
            if( $VERBOSE>=2 ){ print "  XRD file: $fullpath\n"; }
        }
        
        push @DATAFILES, $fullpath;
    }
    
    $DATA = "get_file_list.pl: $cond";
}
else{
    if( !(-e "$DATA") ){ 
        print "ERROR: Data path '$DATA' does not exist\n";
        HelpMessage(0); 
    }
    else{
        if( -f "$DATA" ){
            if( $DATA =~ /\.list$/ ){
                open(my $lfh, '<', $DATA) or die "Could not open '$DATA': $!";
                while(my $line = <$lfh>){
                    chomp $line;
                    next if $line eq "" || $line =~ /^#/;
                    push @DATAFILES, $line;
                }
                close $lfh;
            }
            else{
                push @DATAFILES, $DATA;
            }
        }
        elsif( -d "$DATA" ){
            opendir my $dh, "$DATA" or die "Could not open directory '$DATA': $!";
            my @files = grep { /\.root$/ && -f "$DATA/$_" } readdir($dh);
            closedir $dh;
            
            foreach my $file (@files){
                push @DATAFILES, "$DATA/$file";
            }
        }
    }
}

if( @DATAFILES == 0 ){
    print "ERROR: No data files found\n";
    HelpMessage(0);
}

# Apply file-count limits: explicit -N overrides test default; if not set, test mode limits to 5
if( defined $NFILES ){
    if( $NFILES > 0 && @DATAFILES > $NFILES ){
        @DATAFILES = @DATAFILES[0..($NFILES-1)];
    }
}
elsif( $TEST ){
    @DATAFILES = @DATAFILES[0..4] if @DATAFILES > 5;
}

if( $VERBOSE>=1 ){ print "Found " . scalar(@DATAFILES) . " data files\n"; }

my $totalinputfiles = scalar @DATAFILES;

# Setup output directory
if( !(-d "$OUTDIR") ){ system("/bin/mkdir -p $OUTDIR") == 0 or die "Unable to make '$OUTDIR': $!"; }
$OUTDIR = abs_path($OUTDIR);
my $char = chop $OUTDIR;
while( $char eq "/" ){$char = chop $OUTDIR;}
$OUTDIR = $OUTDIR.$char;

if( $VERBOSE>=1 ){ print "OUTDIR=$OUTDIR\n"; }

# Generate UUID
my $UUID_short = "";
if( ! $UUID ){
    $UUID = $TEST ? "TEST\n" : uc(`uuidgen`);
    chomp($UUID);
    $UUID =~ s/-//g;
    $UUID_short = substr($UUID,0,7);
}
else{
    my $char = chop $UUID;
    while( $char eq "/" ){$char = chop $UUID;}
    $UUID = $UUID.$char;
    $UUID_short = $UUID;
}

if( $VERBOSE>=1 ){print "Job Id: $UUID\n"};

my $FileLoc = "$OUTDIR/$UUID_short";

if (! -e "$FileLoc") {
    system("/bin/mkdir -p $FileLoc") == 0 or die "Unable to make '$FileLoc': $!";
}
else{
    if( $FORCE ){
        system("/bin/rm -r $FileLoc/*") == 0 or die "Unable to remove files in '$FileLoc': $!";
    }
    else{
        print "Directory '$FileLoc' already exists\n";
        print "Remove and overwrite (Y/n): ";
        my $removeinput = <STDIN>; chomp $removeinput;
        if( $removeinput ne "Y" && $removeinput ne "" ){
            print "Quitting to preserve existing folder\n";
            exit(1);
        }
        system("/bin/rm -r $FileLoc/*") == 0 or die "Unable to remove files in '$FileLoc': $!";
    }
}

# Get time
my $epochtime = time();
my $localtime = localtime();

# Create JobWriter
my $JobWriter = new CondorJobWriter($FileLoc, "RunAnalysis.csh", "", $UUID_short);
if( $ALMA9JOB ){ $JobWriter->SetAlmaJob(1); }
$JobWriter->MakeJobDirs($FORCE);
my $CondorDir = $JobWriter->GetCondorDir();
my $ListDir = "$CondorDir/lists";
system("/bin/mkdir -p $ListDir") == 0 or die "Unable to make '$ListDir': $!";

if( $NOWRITESTDOUT ){ $JobWriter->WriteStdOut(0); }
if( $NOWRITESTDERR ){ $JobWriter->WriteStdErr(0); }

# Create summary file
my $FileSummary = "$FileLoc/Summary_${UUID}.list";
# Ensure the directory exists
system("/bin/mkdir -p " . $FileLoc) == 0 or die "Unable to create directory '$FileLoc': $!";
open( my $fh_sum, '>', $FileSummary ) or die "Could not open file '$FileSummary' for writing: $!";

print $fh_sum "UUID: $UUID\n";
print $fh_sum "Epoch Time: $epochtime\n";
print $fh_sum "Time: $localtime\n";
print $fh_sum "Main directory: $LOC\n";
print $fh_sum "Analysis directory: $ANADIR\n";
print $fh_sum "Data: $DATA\n";
print $fh_sum "Output: $OUTDIR\n";
print $fh_sum "Script: runAnalysis.C\n";
print $fh_sum "Verbose: $VERBOSE\n";
print $fh_sum "Node: $ENV{HOST}\n";
if( $MSG ){ print $fh_sum "Message: $MSG\n"; }
print $fh_sum "Command: $thiscommand\n";
if( $GETFILELIST_CMD ){ print $fh_sum "get_file_list_cmd: $GETFILELIST_CMD\n"; }
print $fh_sum "Total input files: $totalinputfiles\n";

# Write shell script
print "Creating RunAnalysis.csh\n";
WriteAnalysisShellMacro("$CondorDir/RunAnalysis.csh");

# Copy necessary files
system("/bin/cp $ANADIR/runAnalysis.C $CondorDir") == 0 or die "Unable to copy 'runAnalysis.C': $!";
$JobWriter->AddInputFiles("$CondorDir/runAnalysis.C");

if( $VALIDATE_MUDSTS ){
    system("/bin/cp $LOC/validate_mudsts.C $CondorDir") == 0 or die "Unable to copy 'validate_mudsts.C': $!";
    $JobWriter->AddInputFiles("$CondorDir/validate_mudsts.C");
}

# Copy compiled libraries if they exist
my $starlibloc = "." . $ENV{'STAR_HOST_SYS'};
if( -d "$ANADIR/$starlibloc" ){
    system("/bin/cp -L -r $ANADIR/$starlibloc $CondorDir") == 0 or die "Unable to copy libraries: $!";
    $JobWriter->AddInputFiles("$CondorDir/$starlibloc");
}

# Add data files and create jobs
if( $VERBOSE>=1 ){print "Grouping files by run number and creating condor jobs\n";}

# Snapshot the common input files (runAnalysis.C, libraries, etc.) before the per-job
# list files are added, so each job gets exactly: common files + its own list file.
my $common_input_files = $JobWriter->SetInputFiles();

my $numfiles = 0; # number of batches/run groups
my $totalevents = 0;

# Group files by run number (fallback to 'unknown' when run not found)
my %files_by_run;
foreach my $datafile (@DATAFILES){
    my $basename = `basename "$datafile"`;
    chomp($basename);
    my $run = 'unknown';

    # Prefer the standard MuDst naming pattern: st_physics_<run>_... or similar
    if( $basename =~ /st_physics_(\d+)_/ ){ 
        $run = $1; 
    }
    # Fallback: any _<7-8 digit>_ sequence (covers other naming schemes)
    elsif( $basename =~ /_(\d{7,8})_/ ){ 
        $run = $1; 
    }

    push @{ $files_by_run{$run} }, $datafile;
    if( $VERBOSE>=2 ){ print "  [group] run=$run <- $basename\n"; }
}

# Create one job per run group (or per batch within a run if batch mode is on)
if( $BATCH && $VERBOSE>=1 ){ print "Batch mode on: up to $BATCHSIZE files per job\n"; }

foreach my $run ( sort keys %files_by_run ){
    my @files = @{ $files_by_run{$run} };
    next unless @files;

    # Subdivide the run's files into batches
    my @batches;
    if( $BATCH ){
        my @remaining = @files;
        while( @remaining ){
            push @batches, [ splice(@remaining, 0, $BATCHSIZE) ];
        }
    }
    else{
        @batches = ( \@files );
    }

    my $nbatches = scalar @batches;
    if( $VERBOSE>=1 && $BATCH ){
        print "  Run $run: " . scalar(@files) . " files -> $nbatches batch(es)\n";
    }

    my $batch_idx = 0;
    foreach my $batch_ref (@batches){
        $batch_idx++;
        my @batch_files   = @$batch_ref;
        my $nfiles_in_batch = scalar @batch_files;
        my $nevents = $NEVENTS;

        my $batch_suffix = $BATCH ? sprintf("_%03d", $batch_idx) : "";
        my $listfile = "$ListDir/Files_${run}${batch_suffix}.list";
        open(my $lfh, '>', $listfile) or die "Could not write $listfile: $!";
        foreach my $f (@batch_files){ print $lfh "$f\n"; }
        close $lfh;

        $JobWriter->SetInputFiles("$common_input_files,$listfile");

        $totalevents += $nevents * $nfiles_in_batch;
        $numfiles++;

        if( $VERBOSE>=2 ){
            my $label = $BATCH ? "run $run batch $batch_idx/$nbatches" : "run $run";
            print "  Creating job for $label with $nfiles_in_batch files\n";
        }

        my $outfile = "${UUID_short}_${run}${batch_suffix}";
        $JobWriter->SetArguments("$nfiles_in_batch $nevents $listfile $outfile");
        $JobWriter->WriteJob();

        print $fh_sum "$listfile\n";
        foreach my $f (@batch_files){ print $fh_sum "  $f\n"; }
    }
}

$JobWriter->CloseJob($numfiles);

# Inject "description = $(D)" into every generated .job file so condor_q shows
# the run/batch identifier (the 4th argument, the output prefix) in the CMD column.
my @jobfiles = glob("$CondorDir/condor_${UUID_short}*.job");
foreach my $jobfile (@jobfiles){
    open(my $jfh_in, '<', $jobfile) or die "Could not read '$jobfile': $!";
    my @lines = <$jfh_in>;
    close $jfh_in;
    open(my $jfh_out, '>', $jobfile) or die "Could not write '$jobfile': $!";
    foreach my $line (@lines){
        print $jfh_out $line;
        if( $line =~ /^Arguments\s*=/ ){
            print $jfh_out "description  = \$(D)\n";
        }
    }
    close $jfh_out;
    if( $VERBOSE>=1 ){ print "Added job description to $jobfile\n"; }
}

# Finalize summary
print $fh_sum "Total input files: $totalinputfiles\n";
print $fh_sum "Total batches: $numfiles\n";
print $fh_sum "Total events: $totalevents\n";
close $fh_sum;

print "\n========================================\n";
print "Job Summary\n";
print "========================================\n";
print "Total input files: $totalinputfiles\n";
print "Total batches: $numfiles\n";
print "Total events: $totalevents\n";
print "Job ID: $UUID_short\n";
print "Location: $FileLoc\n";
print "Summary file: $FileSummary\n";
print "========================================\n";

if( $FORCE ){
    print "Force option is on. Submitting job\n";
    $JobWriter->SubmitJob();
}
else{
    print "Submit Job (Y/n):";
    my $submitinput = <STDIN>; chomp $submitinput;
    if( $submitinput eq "Y" || $submitinput eq "" ){ $JobWriter->SubmitJob(); }
}

sub WriteAnalysisShellMacro
{
    my ($filename) = @_;
    my $validate_block = "";
    if( $VALIDATE_MUDSTS ){
        $validate_block = <<'EOF';
     # Validate the list on the worker node before analysis
     set validlist = "${4}_good_mudsts.list"
     set failedlist = "${4}_failed_mudsts.list"
     echo "Validating input list with validate_mudsts.C"
     root4star -b -q validate_mudsts.C'('\"${3}\"','\"${validlist}\"',false,false,'\"${failedlist}\"')'
     if ( ! -f $validlist ) then
         echo "ERROR: validation did not produce a good-file list"
         exit 1
     endif
     if ( -f $failedlist && -s $failedlist ) then
         echo "WARNING: some files could not be opened (see $failedlist)"
     endif
     if ( ! -s $validlist ) then
         echo "No good MuDst files found after validation; skipping job"
         exit 0
     endif
EOF
    }
    
    my $macro_text = <<"EOF";
#!/bin/csh

stardev
echo \$STAR_LEVEL

# Arguments:
# \$1 = number of files
# \$2 = number of events
# \$3 = input MuDst file or list
# \$4 = output file name

echo "========================================="
echo "Running runAnalysis.C"
echo "========================================="
echo "NumFiles: \${1}"
echo "NumEvents: \${2}"
echo "Input file: \${3}"
echo "Output file: \${4}"
echo "========================================="

# Create temp directory for file processing
set tempdir = "\$SCRATCH"
if( ! -d \$tempdir ) then
    mkdir -p \$tempdir
endif

# Get basename of input file
set inputname = `echo \$3 | awk '{n=split(\$0,a,"/"); print a[n]}'`
echo "Input basename: \$inputname"

# If the input is a list file, don't copy: the list will be transferred with the job
if ( "\$inputname" =~ "*.list" ) then
    echo "Input is a list file: \$inputname (no copy)"
${validate_block}
    set analysislist = "\$inputname"
EOF

    if( $VALIDATE_MUDSTS ) {
        $macro_text .= <<'EOF';
    set analysislist = "$validlist"
EOF
    }

    $macro_text .= <<"EOF";
    # Run the analysis using the selected list file
    echo 'Running: root4star -b -q runAnalysis.C with 4 args'
    root4star -b -q runAnalysis.C'('\${1}','\\"\${analysislist}\\"','\${2}','\\"\${4}\\"')'
    # Check outputs (runAnalysis.C produces outputPrefix_qa.root and outputPrefix_candidates.root)
    if( -f \$4_qa.root ) then
        echo "Output QA created successfully: \$4_qa.root"
    else
        echo "ERROR: QA output was not created!"
    endif
    if( -f \$4_candidates.root ) then
        echo "Output candidates created successfully: \$4_candidates.root"
    else
        echo "ERROR: Candidates output was not created!"
    endif
else
    # Copy input file to temp directory if needed
    if( ! -f \$tempdir/\$inputname ) then
        echo "Copying \$3 to \$tempdir/\$inputname"
        xrdcp -v \$3 \$tempdir/\$inputname
    endif

    # Verify file exists
    if( -f \$tempdir/\$inputname ) then
        echo "Input file verified in temp directory"
        
        # Run the analysis
        echo 'Running: root4star -b -q runAnalysis.C with 4 args'
        root4star -b -q "runAnalysis.C(\${1},\"\${tempdir}/\${inputname}\",\${2},\"\${4}\")"
        
        # Check if output was created (runAnalysis.C should create the _qa and _candidates files)
        if( -f \$4_qa.root ) then
            echo "Output QA created successfully: \$4_qa.root"
        else
            echo "ERROR: QA output was not created!"
        endif
        if( -f \$4_candidates.root ) then
            echo "Output candidates created successfully: \$4_candidates.root"
        else
            echo "ERROR: Candidates output was not created!"
        endif
        
        # Cleanup temp file
        rm -v \$tempdir/\$inputname
    else
        echo "ERROR: Input file copy failed or does not exist!"
        exit 1
    endif
endif

echo "========================================="
echo "Analysis complete"
echo "========================================="
EOF

    open(my $fh, '>', $filename) or die "Could not open '$filename' for writing: $!";
    print $fh $macro_text;
    close($fh);
    system("chmod +x $filename");
}