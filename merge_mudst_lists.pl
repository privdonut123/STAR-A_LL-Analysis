#!/usr/bin/perl

use strict;
use warnings;
use Getopt::Long qw(GetOptions HelpMessage);
use Pod::Usage;

Getopt::Long::Configure('no_ignore_case');

=pod

=head1 NAME

merge_mudst_lists - Merge all *_good_mudsts.list and *_failed_mudsts.list files from a validation output directory

=head1 SYNOPSIS

merge_mudst_lists.pl -d I<output_dir> [-g I<good.list>] [-f I<failed.list>] [-v I<level{1}>] [-h]

=over 4

=item B<-d> directory containing *_good_mudsts.list and *_failed_mudsts.list files (required)

=item B<-g> output merged good list (default: <dir>/merged_good_mudsts.list)

=item B<-f> output merged failed list (default: <dir>/merged_failed_mudsts.list)

=item B<-v> verbosity level (default 1; 2=per-file counts; 3=per-line trace)

=item B<-h> print this help and exit

=back

=head1 DESCRIPTION

Finds all files matching *_good_mudsts.list and *_failed_mudsts.list inside
the given directory, strips comment lines, deduplicates entries, and writes
two merged output files.  Failed entries with inline comments (e.g. "# cannot
open") are deduplicated by filepath only so the same file appearing in
multiple per-run lists is counted once.

=cut

my $DIR         = "";
my $GOOD_OUT    = "";
my $FAILED_OUT  = "";
my $VERBOSE     = 1;

GetOptions(
    'dir|d=s'        => \$DIR,
    'good|g=s'       => \$GOOD_OUT,
    'failed|f=s'     => \$FAILED_OUT,
    'verbose|v=i'    => \$VERBOSE,
    'help|h'         => sub { HelpMessage(0) },
) or HelpMessage(1);

if( $DIR eq "" ){
    print "ERROR: Must specify -d <directory>\n";
    HelpMessage(0);
}

if( !(-d $DIR) ){
    print "ERROR: Directory '$DIR' does not exist\n";
    exit(1);
}

$DIR =~ s|/+$||;

$GOOD_OUT   = "$DIR/merged_good_mudsts.list"   if $GOOD_OUT   eq "";
$FAILED_OUT = "$DIR/merged_failed_mudsts.list" if $FAILED_OUT eq "";

if( $VERBOSE >= 2 ){
    print "DEBUG: verbosity=$VERBOSE\n";
    print "DEBUG: source directory: $DIR\n";
    print "DEBUG: good output:      $GOOD_OUT\n";
    print "DEBUG: failed output:    $FAILED_OUT\n";
}

# ---------------------------------------------------------------------------
# merge_lists($glob_pattern, $header, $outfile, $strip_inline_comment)
#   Reads all files matching $glob_pattern, skips blank/comment lines,
#   deduplicates, and writes a merged output file.
#   When $strip_inline_comment is true, deduplication key is the filepath
#   portion only (text before the first "  #"), so "file.root  # reason"
#   and "file.root  # other reason" are treated as the same entry.
# ---------------------------------------------------------------------------
sub merge_lists {
    my ($glob_pattern, $header, $outfile, $strip_inline_comment) = @_;

    my @listfiles = sort glob($glob_pattern);

    if( $VERBOSE >= 2 ){
        print "DEBUG: glob pattern: $glob_pattern\n";
        print "DEBUG: files found:  " . scalar(@listfiles) . "\n";
        if( $VERBOSE >= 3 ){
            print "DEBUG: list files to merge:\n";
            foreach my $lf (@listfiles){ print "  $lf\n"; }
        }
    }

    if( @listfiles == 0 ){
        print "WARNING: No files matching '$glob_pattern' found — skipping\n";
        return (0, 0, 0, 0);
    }

    if( $VERBOSE >= 1 ){
        print "Found " . scalar(@listfiles) . " file(s) matching $glob_pattern\n";
        print "Output file: $outfile\n";
    }

    my %seen;
    my @merged;
    my $total_read    = 0;
    my $total_blank   = 0;
    my $total_comment = 0;
    my $total_dupes   = 0;
    my $nfiles_done   = 0;

    foreach my $lf (@listfiles){
        $nfiles_done++;

        if( $VERBOSE == 1 && $nfiles_done % 100 == 0 ){
            print "  ... processed $nfiles_done / " . scalar(@listfiles) . " files\n";
        }
        if( $VERBOSE >= 2 ){
            print "  [$nfiles_done/" . scalar(@listfiles) . "] Reading: $lf\n";
        }

        open(my $fh, '<', $lf) or die "Could not open '$lf': $!";

        my ($file_entries, $file_blank, $file_comment, $file_dupes, $linenum) = (0,0,0,0,0);

        while( my $line = <$fh> ){
            $linenum++;
            chomp $line;

            if( $line =~ /^\s*$/ ){
                $file_blank++;
                $total_blank++;
                if( $VERBOSE >= 3 ){ print "    [line $linenum] BLANK\n"; }
                next;
            }
            if( $line =~ /^\s*#/ ){
                $file_comment++;
                $total_comment++;
                if( $VERBOSE >= 3 ){ print "    [line $linenum] COMMENT: $line\n"; }
                next;
            }

            $total_read++;

            # Dedup key: strip trailing inline comment for failed files so
            # "file.root  # cannot open" and "file.root  # no MuDst tree"
            # both map to the same filepath key.
            my $key = $strip_inline_comment ? ($line =~ s/\s*#.*$//r) : $line;

            if( $seen{$key}++ ){
                $file_dupes++;
                $total_dupes++;
                if( $VERBOSE >= 2 ){ print "    [line $linenum] DUPE: $line\n"; }
                next;
            }

            push @merged, $line;
            $file_entries++;
            if( $VERBOSE >= 3 ){ print "    [line $linenum] OK: $line\n"; }
        }
        close $fh;

        if( $VERBOSE >= 2 ){
            print "    -> $file_entries unique  |  $file_dupes dupe(s)  |  $file_comment comment(s)  |  $file_blank blank(s)\n";
        }
    }

    my $total_unique = scalar @merged;

    if( $VERBOSE >= 2 ){
        print "DEBUG: finished reading all files\n";
        print "DEBUG: total lines read (non-comment): $total_read\n";
        print "DEBUG: total blank lines skipped:      $total_blank\n";
        print "DEBUG: total comment lines skipped:    $total_comment\n";
        print "DEBUG: total duplicates removed:       $total_dupes\n";
        print "DEBUG: total unique entries to write:  $total_unique\n";
        print "DEBUG: opening output file for writing: $outfile\n";
    }

    open(my $out, '>', $outfile) or die "Could not open '$outfile' for writing: $!";
    print $out "$header\n";
    print $out "# Source directory: $DIR\n";
    print $out "# Source files: " . scalar(@listfiles) . "\n";
    print $out "# Total entries: $total_unique\n";
    print $out "#\n";
    foreach my $entry (@merged){ print $out "$entry\n"; }
    close $out;

    if( $VERBOSE >= 2 ){ print "DEBUG: output file written and closed\n"; }

    return (scalar(@listfiles), $total_read, $total_dupes, $total_unique);
}

# ---------------------------------------------------------------------------
# Run merges
# ---------------------------------------------------------------------------
print "\n--- Merging good MuDst files ---\n";
my ($g_nfiles, $g_read, $g_dupes, $g_unique) = merge_lists(
    "$DIR/*_good_mudsts.list",
    "# Merged good MuDst files with EEMC data",
    $GOOD_OUT,
    0,   # exact-line dedup (no inline comments in good lists)
);

print "\n--- Merging failed MuDst files ---\n";
my ($f_nfiles, $f_read, $f_dupes, $f_unique) = merge_lists(
    "$DIR/*_failed_mudsts.list",
    "# Merged failed MuDst files (could not open or no MuDst tree)",
    $FAILED_OUT,
    1,   # filepath-only dedup (entries have trailing "# reason" comments)
);

print "\n========================================\n";
print "Merge Summary\n";
print "========================================\n";
printf "%-25s  %6s  %6s  %6s  %6s\n", "Type", "Files", "Read", "Dupes", "Unique";
printf "%-25s  %6d  %6d  %6d  %6d\n", "Good MuDsts",   $g_nfiles, $g_read, $g_dupes, $g_unique;
printf "%-25s  %6d  %6d  %6d  %6d\n", "Failed MuDsts", $f_nfiles, $f_read, $f_dupes, $f_unique;
print "----------------------------------------\n";
print "Good output:    $GOOD_OUT\n";
print "Failed output:  $FAILED_OUT\n";
print "========================================\n";
