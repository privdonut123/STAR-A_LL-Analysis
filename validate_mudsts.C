/*
 * validate_mudsts.C
 * 
 * Validates MuDst files to check if they contain EEMC data.
 * Outputs a list of "good" files (those with EEMC content).
 *
 * Checks MuDst tree for EEMC branches:
 *   - EEmcPrs
 *   - EEmcSmdu
 *   - EEmcSmdv
 *
 * Usage:
 *   // Validate from text file list
 *   root -b -q 'validate_mudsts.C("input_file_list.txt", "output_good_files.list")'
 *
 *   // Validate from directory
 *   root -b -q 'validate_mudsts.C("/path/to/mudst_dir", "output_good_files.list", true)'
 *
 *   // Validate using get_file_list.pl query
 *   root -b -q 'validate_mudsts.C("get_file_list.pl -keys path,filename -delim / -limit 100 -cond ...", "output_good_files.list", false, true)'
 */

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <cstdlib>
#include <TFile.h>
#include <TTree.h>
#include <TBranch.h>
#include <TSystemDirectory.h>
#include <TSystemFile.h>
#include <TSystem.h>
#include <unistd.h>

void validate_mudsts(
    const char* inputPath = "input_files.list",
    const char* outputFile = "good_mudsts.list",
    bool isDirectory = false,
    bool useGetFileList = false,
    const char* failedOutputFile = "failed_mudsts.list"
) {
    // Suppress ROOT warnings about missing class dictionaries
    gErrorIgnoreLevel = kWarning + 1;
    
    cout << "==================================================" << endl;
    cout << "MuDst EEMC Validator" << endl;
    cout << "==================================================" << endl;
    
    vector<string> fileList;
    
    // Get list of files to validate
    if (useGetFileList) {
        // This path shouldn't be used anymore - Perl script handles get_file_list.pl directly
        cout << "Error: useGetFileList mode should not be called from Perl script" << endl;
        return;
    } else if (string(inputPath).find(".MuDst.root") != string::npos) {
        // Single MuDst file provided directly
        cout << "Single MuDst file detected: " << inputPath << endl;
        fileList.push_back(inputPath);
    } else if (isDirectory) {
        cout << "Reading MuDst files from directory: " << inputPath << endl;
        TSystemDirectory dir(inputPath, inputPath);
        cout << "Scanning directory for .MuDst.root files..." << endl;
        TList* files = dir.GetListOfFiles();
        cout << "Total files found in directory: " << (files ? files->GetEntries() : 0) << endl;
        
        if (!files) {
            cerr << "Error: Could not read directory" << endl;
            return;
        }
        
        TIter next(files);
        TSystemFile* file;
        while ((file = (TSystemFile*)next())) {
            string fileName = file->GetName();
            if (fileName.find(".MuDst.root") != string::npos) {
                string fullPath = string(inputPath) + "/" + fileName;
                fileList.push_back(fullPath);
            }
        }
    } else {
        // Read from text file
        cout << "Reading file list from: " << inputPath << endl;
        ifstream infile(inputPath);
        if (!infile.is_open()) {
            cerr << "Error: Could not open input file: " << inputPath << endl;
            return;
        }
        else {
            cout << "Successfully opened input file: " << inputPath << endl;
        }

        string line;
        int lineNum = 0;
        while (getline(infile, line)) {
            lineNum++;
            cout << "[Line " << lineNum << "] Raw: '" << line << "'" << endl;
            // Trim trailing CR and spaces
            while (!line.empty() && (line[line.size() - 1] == '\r' || line[line.size() - 1] == '\n' || line[line.size() - 1] == ' ' || line[line.size() - 1] == '\t')) line.erase(line.size() - 1);
            // Trim leading whitespace
            size_t start = 0;
            while (start < line.size() && isspace((unsigned char)line[start])) start++;
            if (start > 0) line = line.substr(start);
            cout << "[Line " << lineNum << "] After trim: '" << line << "'" << endl;

            if (line.empty() || line[0] == '#') {
                cout << "[Line " << lineNum << "] Skipping (empty or comment)" << endl;
                continue;
            }

            // If the line does not contain a complete .MuDst.root filename, attempt to append following lines
            string combined = line;
            int appendCount = 0;
            while (combined.find(".MuDst.root") == string::npos) {
                string nextLine;
                if (!getline(infile, nextLine)) {
                    cout << "  No more lines to read (EOF)" << endl;
                    break;
                }
                appendCount++;
                cout << "  Appending line " << appendCount << ": '" << nextLine << "'" << endl;
                // Trim nextLine as well
                while (!nextLine.empty() && (nextLine[nextLine.size() - 1] == '\r' || nextLine[nextLine.size() - 1] == '\n' || nextLine[nextLine.size() - 1] == ' ' || nextLine[nextLine.size() - 1] == '\t')) nextLine.erase(nextLine.size() - 1);
                size_t s2 = 0;
                while (s2 < nextLine.size() && isspace((unsigned char)nextLine[s2])) s2++;
                if (s2 > 0) nextLine = nextLine.substr(s2);
                if (nextLine.empty() || nextLine[0] == '#') {
                    cout << "  Stopping append (empty or comment)" << endl;
                    break;
                }
                combined += nextLine;
                cout << "  Combined so far: '" << combined << "'" << endl;
            }

            cout << "[Line " << lineNum << "] Final: '" << combined << "'" << endl;
            fileList.push_back(combined);
        }
        infile.close();
    }
    
    cout << "Found " << fileList.size() << " files to validate" << endl << endl;
    
    // Validate each file for EEMC data
    vector<string> goodFiles;
    vector<string> failedFiles;
    int fileCount = 0;
    int goodCount = 0;
    
    for (int i = 0; i < fileList.size(); i++) {
        string filePath = fileList[i];
        fileCount++;
        cout << "[" << fileCount << "/" << fileList.size() << "] Validating: " << filePath << " ... ";
        cout.flush();
        
        cout << endl << "  Opening with ROOT: " << filePath << endl;
        TFile* file = TFile::Open(filePath.c_str(), "READ");
        if (!file || file->IsZombie()) {
            cout << "FAIL (cannot open)" << endl;
            failedFiles.push_back(filePath + "  # cannot open");
            continue;
        }
        cout << "  File opened successfully" << endl;
        
        bool hasEEmcData = false;
        
        // Get the MuDst tree
        TTree* mudstTree = (TTree*)file->Get("MuDst");
        if (!mudstTree) {
            cout << "FAIL (MuDst tree not found)" << endl;
            failedFiles.push_back(filePath + "  # no MuDst tree");
            file->Close();
            delete file;
            continue;
        }
        cout << "  MuDst tree found, entries: " << mudstTree->GetEntries() << endl;
        
        // Check for EEMC branches within MuDst tree
        vector<string> eemcBranches;
        eemcBranches.push_back("EEmcPrs");
        eemcBranches.push_back("EEmcSmdu");
        eemcBranches.push_back("EEmcSmdv");
        for (int b = 0; b < eemcBranches.size(); b++) {
            string branchName = eemcBranches[b];
            cout << "  Checking branch: " << branchName << endl;
            TBranch* branch = mudstTree->GetBranch(branchName.c_str());
            if (branch) {
                long entries = branch->GetEntries();
                cout << "    Found, entries: " << entries << endl;
                if (entries > 0) {
                    hasEEmcData = true;
                    cout << "    -> HAS DATA" << endl;
                    break;
                }
            } else {
                cout << "    Not found" << endl;
            }
        }
        
        file->Close();
        delete file;
        
        if (hasEEmcData) {
            cout << "GOOD" << endl;
            goodFiles.push_back(filePath);
            goodCount++;
        } else {
            cout << "SKIP (no EEMC data)" << endl;
        }
    }
    
    // Write output file
    cout << "\n==================================================" << endl;
    cout << "Writing good files to: " << outputFile << endl;
    
    ofstream outfile(outputFile);
    if (!outfile.is_open()) {
        cerr << "Error: Could not open output file: " << outputFile << endl;
        return;
    }
    
    outfile << "# Good MuDst files with EEMC data" << endl;
    outfile << "# Total: " << goodCount << " / " << fileCount << endl;
    outfile << "#" << endl;
    
    for (int i = 0; i < goodFiles.size(); i++) {
        outfile << goodFiles[i] << endl;
    }
    
    outfile.close();

    // Write failed files list
    cout << "Writing failed files to: " << failedOutputFile << endl;
    ofstream failfile(failedOutputFile);
    if (!failfile.is_open()) {
        cerr << "Error: Could not open failed output file: " << failedOutputFile << endl;
    } else {
        failfile << "# MuDst files that could not be opened or had no MuDst tree" << endl;
        failfile << "# Total: " << failedFiles.size() << " / " << fileCount << endl;
        failfile << "#" << endl;
        for (int i = 0; i < failedFiles.size(); i++) {
            failfile << failedFiles[i] << endl;
        }
        failfile.close();
    }

    cout << "==================================================" << endl;
    cout << "Summary:" << endl;
    cout << "  Total files:   " << fileCount << endl;
    cout << "  Good files:    " << goodCount << endl;
    cout << "  Failed files:  " << failedFiles.size() << endl;
    cout << "  Skipped files: " << (fileCount - goodCount - (int)failedFiles.size()) << endl;
    cout << "  Good output:   " << outputFile << endl;
    cout << "  Failed output: " << failedOutputFile << endl;
    cout << "==================================================" << endl;
}
