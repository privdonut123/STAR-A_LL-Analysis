// Forward declarations for EEMC Part 1 calibration
class StEEmcDbMaker;
class StEEmcA2EMaker;
class StEEmcEnergyMaker_t;
class StEEmcHitMakerSimple_t;
class StEEmcStripClusterFinderIU_t;
class StEEmcPointFinderIU_t;
class StEEmcEnergyApportionerIU_t;
class StSpinInfoMaker_t;
class St_db_Maker;
class StMuDst2StEventMaker;

void runAnalysis(Int_t nFiles = 1,
                 TString InputFileList = "/star/u/bmagh001/MuDst/input/st_fwd_22356022_raw_2500032.MuDst.root",
                 Int_t nEvents = 10000,
                 TString outputPrefix = "pi0_analysis",
                 Int_t debug = 0)
{
    cout << "========================================" << endl;
    cout << "Pi0 Reconstruction using EEMC Part 1 Energy Maker" << endl;
    cout << "========================================" << endl;
    
    cout << "Input file: " << InputFileList << endl;
    cout << "Number of events: " << nEvents << endl;
    cout << "Output prefix: " << outputPrefix << endl;
    cout << "Debug level: " << debug << endl;
    cout << "========================================" << endl;
    
    // =====================================================================
    // LIBRARY LOADING
    // =====================================================================
    cout << "\n>>> Loading STAR libraries..." << endl;
    
    gROOT->LoadMacro("$STAR/StRoot/StMuDSTMaker/COMMON/macros/loadSharedLibraries.C");
    loadSharedLibraries();
    
    // Database and EEMC libraries for Part 1
    cout << "Loading database and EEMC libraries..." << endl;
    gSystem->Load("StDetectorDbMaker");
    gSystem->Load("StDbUtilities");
    gSystem->Load("StDbBroker");
    gSystem->Load("St_db_Maker");
    gSystem->Load("StEEmcUtil");
    gSystem->Load("StEEmcDbMaker");
    gSystem->Load("StEEmcA2EMaker");
    gSystem->Load("StSpinDbMaker");
    gSystem->Load("StTriggerUtilities");
    gSystem->Load("StEEmcPoolEEmcTreeContainers");
    gSystem->Load("StEEmcTreeMaker");
    gSystem->Load("StEEmcHitMaker");
    gSystem->Load("StEEmcPool");
    
    if (debug < 1) {
        gMessMgr->SetLimit("I",0); //Turn off log info messages
        gMessMgr->SetLimit("Q",0); //turn off log warn messages
        gMessMgr->SetLimit("W",0);
    }
    
    TString libPath = gSystem->GetDynamicPath();
    libPath = "./.sl73_x8664_gcc485/lib:" + libPath;
    gSystem->SetDynamicPath(libPath.Data());
    
    // Load the Pi0FinderForEEmc library
    cout << "Loading Pi0FinderForEEmc library..." << endl;
    Int_t loadStatus = gSystem->Load("./.sl73_x8664_gcc485/lib/libStPi0FinderForEEmc.so");
    if (loadStatus != 0) {
        cout << "ERROR: Failed to load libStPi0FinderForEEmc.so (status: " << loadStatus << ")" << endl;
        return;
    }
    
    cout << "All libraries loaded successfully\n" << endl;
    
    // =====================================================================
    // CHAIN INITIALIZATION
    // =====================================================================
    cout << ">>> Creating analysis chain..." << endl;
    StChain *chain = new StChain("pi0AnalysisChain");
    
    // =====================================================================
    // PART 1: ADC to Calibrated Energy
    // =====================================================================
    cout << "\n>>> ========== PART 1: ADC to Calibrated Energy ==========" << endl;
    
    cout << "    Creating StMuDstMaker..." << endl;
    StMuDstMaker *muDstMaker = new StMuDstMaker(0, 0, "", InputFileList.Data(), "MuDst", nFiles);
    // muDstMaker->SetDebug(debug);
    muDstMaker->SetStatus("*", 0);
    muDstMaker->SetStatus("*Event*", 1);
    muDstMaker->SetStatus("PrimaryVertices", 1);
    muDstMaker->SetStatus("EmcAll", 1);
    chain->AddMaker(muDstMaker);
    
    cout << "    Creating StMuDst2StEventMaker..." << endl;
    StMuDst2StEventMaker *muDst2StEvent = new StMuDst2StEventMaker();
    chain->AddMaker(muDst2StEvent);
    
    cout << "    Creating St_db_Maker..." << endl;
    St_db_Maker *dbMk = new St_db_Maker("StarDb", "MySQL:StarDb", "MySQL:StarDb", "$STAR/StarDb");
    dbMk->SetAttr("blacklist", "fgt");
    dbMk->SetAttr("blacklist", "svt");
    dbMk->SetAttr("blacklist", "tpc");
    dbMk->SetAttr("blacklist", "ftpc");
    chain->AddMaker(dbMk);
    
    cout << "    Creating StEEmcDbMaker..." << endl;
    StEEmcDbMaker *eemcDbMaker = new StEEmcDbMaker("eemcDb");
    chain->AddMaker(eemcDbMaker);
    
    cout << "    Creating StEEmcA2EMaker..." << endl;
    StEEmcA2EMaker *a2eMaker = new StEEmcA2EMaker("EEmcA2EMaker");
    a2eMaker->database("eemcDb");
    a2eMaker->source("MuDst", 1);
    a2eMaker->threshold(3.0, 0);  // tower
    a2eMaker->threshold(3.0, 1);  // pre1
    a2eMaker->threshold(3.0, 2);  // pre2
    a2eMaker->threshold(3.0, 3);  // post
    a2eMaker->threshold(3.0, 4);  // smdu
    a2eMaker->threshold(3.0, 5);  // smdv
    chain->AddMaker(a2eMaker);
    
    cout << "    Creating StEEmcEnergyMaker_t..." << endl;
    StEEmcEnergyMaker_t *energyMaker = new StEEmcEnergyMaker_t("energyMkr", "EEmcA2EMaker");
    energyMaker->setStripThres(0.001);
    energyMaker->setTowerThres(1.0);
    chain->AddMaker(energyMaker);
    
    cout << "    Creating cluster finders for Part 2 hit reconstruction..." << endl;
    
    // Tower cluster finder (optional - use NULL to disable)
    // StEEmcTowerClusterFinder_t *towerClusterFinder = NULL;
    
    // Strip cluster finder
    StEEmcStripClusterFinderIU_t *stripClusterFinder = new StEEmcStripClusterFinderIU_t();
    stripClusterFinder->setSeedFloorConst(1);
    
    // Point finder
    StEEmcPointFinderIU_t *pointFinder = new StEEmcPointFinderIU_t();
    
    // Energy apportioner
    StEEmcEnergyApportionerIU_t *energyApportioner = new StEEmcEnergyApportionerIU_t();
    energyApportioner->setCheckTowerBits(0);
    
    // Create hit maker with cluster finders
    cout << "    Creating StEEmcHitMakerSimple_t..." << endl;
    StEEmcHitMakerSimple_t *hitMaker = new StEEmcHitMakerSimple_t("hitMaker",
                                                                   "energyMkr",
                                                                   NULL,
                                                                   stripClusterFinder,
                                                                   pointFinder,
                                                                   energyApportioner);
    hitMaker->doClusterTowers(0);
    hitMaker->doClusterPreShower1(0);
    hitMaker->doClusterPreShower2(0);
    hitMaker->doClusterPostShower(0);
    hitMaker->doClusterSMDStrips(1);
    chain->AddMaker(hitMaker);
    
    cout << "    PART 1-2 complete: ADC converted to calibrated energies and SMD-clustered hits" << endl;
    
    // =====================================================================
    // ANALYSIS: Pi0 Reconstruction
    // =====================================================================
    cout << "\n>>> ========== ANALYSIS: Pi0 Reconstruction ==========" << endl;
    
    cout << "    Creating StPi0FinderForEEmc maker..." << endl;
    StPi0FinderForEEmc *pi0Finder = new StPi0FinderForEEmc("Pi0Finder");
    
    TString qaFile = outputPrefix + "_qa.root";
    TString candFile = outputPrefix + "_candidates.root";
    pi0Finder->SetHistogramFile(qaFile);
    pi0Finder->SetTreeFile(candFile);
    
    pi0Finder->SetPhotonEnergyMin(1);
    pi0Finder->SetPi0EnergyMin(2.0);
    pi0Finder->SetPi0MassMax(1);
    pi0Finder->SetAsymmetryMax(0.8);
    pi0Finder->SetDebugLevel(debug);
    pi0Finder->SetEnergyMaker(energyMaker);
    pi0Finder->SetHitMaker(hitMaker);
    
    chain->AddMaker(pi0Finder);
    
    cout << "    Analysis chain configuration complete" << endl;
    
    // =====================================================================
    // EVENT LOOP
    // =====================================================================
    cout << "\n>>> ========== RUNNING EVENT LOOP ==========" << endl;
    cout << "Input file: " << InputFileList << endl;
    
    cout << "Initializing chain..." << endl;
    Int_t initStatus = chain->Init();
    if (initStatus != 0) {
        cout << "ERROR: Chain initialization failed with status " << initStatus << endl;
        return;
    }
    cout << "Chain initialized successfully" << endl;
    
    cout << "Processing " << nEvents << " events..." << endl;
    chain->EventLoop(nEvents);
    
    cout << "Finishing chain..." << endl;
    chain->Finish();
    
    cout << "Deleting chain..." << endl;
    delete chain;
    
    cout << "\n========================================" << endl;
    cout << "Analysis Complete!" << endl;
    cout << "Output files:" << endl;
    cout << "  - QA Histograms: " << qaFile << endl;
    cout << "  - Pi0 Candidates: " << candFile << endl;
    cout << "========================================" << endl;
}