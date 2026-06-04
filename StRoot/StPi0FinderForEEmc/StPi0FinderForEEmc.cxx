#include "StPi0FinderForEEmc.h"
#include "EEmcPhotonUtils.h"

#include "StMuDSTMaker/COMMON/StMuDstMaker.h"
#include "StMuDSTMaker/COMMON/StMuEvent.h"
#include "StMuDSTMaker/COMMON/StMuDst.h"
#include "StMuDSTMaker/COMMON/StMuPrimaryVertex.h"
#include "StarClassLibrary/StThreeVectorF.hh"
#include "StRoot/StEEmcPool/EEmcTreeContainers/EEmcHit.h"
#include "StEvent/StEnumerations.h"
#include "StEvent/StEvent.h"

#include "StEEmcUtil/EEmcGeom/EEmcGeomSimple.h"
#include "StRoot/StEEmcPool/StEEmcHitMaker/StEEmcHitMakerSimple.h"

#include "TH1.h"
#include "TH2.h"
#include "TMath.h"
#include "TClonesArray.h"

ClassImp(StPi0FinderForEEmc)

//------------------------------------------------------
// Constructor
//------------------------------------------------------
StPi0FinderForEEmc::StPi0FinderForEEmc(const char* name) : StMaker(name)
{
    mMuDstMaker = NULL;
    mEnergyMaker = NULL;
    mHitMaker = NULL;
    mHistogramOutput = NULL;
    mTreeOutput = NULL;
    mPi0Tree = NULL;
    
    // Default output file names
    mHistogramFileName = "pi0_qa.root";
    mTreeFileName = "pi0_candidates.root";
    
    // Default cuts
    mPhotonEnergyMin = 0.05;     // GeV
    mPi0EnergyMin = 0.05;         // GeV
    mPi0MassMax = 1.0;           // GeV (standard pi0 mass window)
    mAsymmetryMax = 0.7;         // zgg cut
    
    mDebug = 1;
}

//------------------------------------------------------
// Destructor
//------------------------------------------------------
StPi0FinderForEEmc::~StPi0FinderForEEmc()
  {
    // Destroy and/or zero out all public/private data members here.
  }

//------------------------------------------------------
// Init: Initialize histograms and output files
//------------------------------------------------------
Int_t StPi0FinderForEEmc::Init()
{
    cout << termcolor::green << "========================================" << termcolor::reset << endl;
    cout << termcolor::green << "Initializing StPi0FinderForEEmc" << termcolor::reset << endl;
    cout << termcolor::green << "========================================" << termcolor::reset << endl;
    
    // Create output files
    mHistogramOutput = new TFile(mHistogramFileName, "RECREATE");
    mTreeOutput = new TFile(mTreeFileName, "RECREATE");
    
    if (!mHistogramOutput || !mTreeOutput) {
        cout << termcolor::red << "ERROR: Could not create output files!" << termcolor::reset << endl;
        return kStErr;
    }
    
    // ========== Photon QA Histograms ==========
    h1_photon_energy = new TH1F("h1_photon_energy", "Photon Energy", 200, 0, 50);
    h1_photon_energy->SetXTitle("Energy [GeV]");
    h1_photon_energy->SetYTitle("Counts");
    
    h1_photon_eta = new TH1F("h1_photon_eta", "Photon Pseudorapidity", 100, 0, 2.5);
    h1_photon_eta->SetXTitle("#eta");
    h1_photon_eta->SetYTitle("Counts");
    
    h1_photon_phi = new TH1F("h1_photon_phi", "Photon Azimuthal Angle", 144, -TMath::Pi(), TMath::Pi());
    h1_photon_phi->SetXTitle("#phi [rad]");
    h1_photon_phi->SetYTitle("Counts");
    
    h2_photon_energy_vs_eta = new TH2F("h2_photon_energy_vs_eta", "Photon Energy vs Pseudorapidity", 
                                       50, 0, 2.5, 80, 0, 40);
    h2_photon_energy_vs_eta->SetXTitle("#eta");
    h2_photon_energy_vs_eta->SetYTitle("Energy [GeV]");
    
    h2_photon_position = new TH2F("h2_photon_position", "Photon Position in EEMC",
                                  200, -250, 250, 200, -250, 250);
    h2_photon_position->SetXTitle("X [cm]");
    h2_photon_position->SetYTitle("Y [cm]");
    
    // ========== Pi0 QA Histograms ==========
    h1_pi0_mass = new TH1F("h1_pi0_mass", "Pi0 Invariant Mass", 150, 0, 0.3);
    h1_pi0_mass->SetXTitle("M [GeV/c^2]");
    h1_pi0_mass->SetYTitle("Counts");
    
    h1_pi0_energy = new TH1F("h1_pi0_energy", "Pi0 Total Energy", 200, 0, 100);
    h1_pi0_energy->SetXTitle("Energy [GeV]");
    h1_pi0_energy->SetYTitle("Counts");
    
    h1_pi0_pt = new TH1F("h1_pi0_pt", "Pi0 Transverse Momentum", 150, 0, 30);
    h1_pi0_pt->SetXTitle("p_{T} [GeV/c]");
    h1_pi0_pt->SetYTitle("Counts");
    
    h1_pi0_eta = new TH1F("h1_pi0_eta", "Pi0 Pseudorapidity", 100, 0, 2.5);
    h1_pi0_eta->SetXTitle("#eta");
    h1_pi0_eta->SetYTitle("Counts");
    
    h1_pi0_opening_angle = new TH1F("h1_pi0_opening_angle", "Opening Angle Between Photons", 180, 0, TMath::Pi());
    h1_pi0_opening_angle->SetXTitle("Opening Angle [rad]");
    h1_pi0_opening_angle->SetYTitle("Counts");
    
    h1_pi0_asymmetry = new TH1F("h1_pi0_asymmetry", "Energy Asymmetry (zgg)", 100, 0, 1.0);
    h1_pi0_asymmetry->SetXTitle("zgg = |E1-E2|/(E1+E2)");
    h1_pi0_asymmetry->SetYTitle("Counts");
    
    h2_pi0_mass_vs_energy = new TH2F("h2_pi0_mass_vs_energy", "Pi0 Mass vs Energy",
                                     100, 0, 100, 150, 0, 0.3);
    h2_pi0_mass_vs_energy->SetXTitle("Energy [GeV]");
    h2_pi0_mass_vs_energy->SetYTitle("M [GeV/c^2]");
    
    h2_pi0_position = new TH2F("h2_pi0_position", "Pi0 Position in EEMC",
                              200, -250, 250, 200, -250, 250);
    h2_pi0_position->SetXTitle("X [cm]");
    h2_pi0_position->SetYTitle("Y [cm]");
    
    h2_detector_hitmap = new TH2F("h2_detector_hitmap", "EEMC Detector Tower Positions",
                                  100, -250, 250, 100, -250, 250);
    h2_detector_hitmap->SetXTitle("X [cm]");
    h2_detector_hitmap->SetYTitle("Y [cm]");
    
    h1_detector_phi = new TH1F("h1_detector_phi", "EEMC Detector Hit Azimuthal Angle", 144, -TMath::Pi(), TMath::Pi());
    h1_detector_phi->SetXTitle("#phi [rad]");
    h1_detector_phi->SetYTitle("Counts");
    
    // ========== Create Pi0 Tree ==========
    mTreeOutput->cd();
    mPi0Tree = new TTree("pi0_tree", "Pi0 Candidate Tree");
    
    // Set branch addresses for the tree
    mPi0Tree->Branch("event_number", &mEventNumber, "event_number/I");
    mPi0Tree->Branch("run_number", &mRunNumber, "run_number/I");
    
    mPi0Tree->Branch("pi0_energy", &mPi0.energy, "pi0_energy/F");
    mPi0Tree->Branch("pi0_mass", &mPi0.mass, "pi0_mass/F");
    mPi0Tree->Branch("pi0_pt", &mPi0.pt, "pi0_pt/F");
    mPi0Tree->Branch("pi0_eta", &mPi0.eta, "pi0_eta/F");
    mPi0Tree->Branch("pi0_phi", &mPi0.phi, "pi0_phi/F");
    mPi0Tree->Branch("opening_angle", &mPi0.openingAngle, "opening_angle/F");
    mPi0Tree->Branch("asymmetry", &mPi0.asymmetry, "asymmetry/F");
    mPi0Tree->Branch("pi0_pos_x", &mPi0.pos_x, "pi0_pos_x/F");
    mPi0Tree->Branch("pi0_pos_y", &mPi0.pos_y, "pi0_pos_y/F");
    mPi0Tree->Branch("pi0_pos_z", &mPi0.pos_z, "pi0_pos_z/F");
    
    // Photon 1 branches
    mPi0Tree->Branch("ph1_energy", &mPi0.ph1_energy, "ph1_energy/F");
    mPi0Tree->Branch("ph1_eta", &mPi0.ph1_eta, "ph1_eta/F");
    mPi0Tree->Branch("ph1_phi", &mPi0.ph1_phi, "ph1_phi/F");
    mPi0Tree->Branch("ph1_pos_x", &mPi0.ph1_pos_x, "ph1_pos_x/F");
    mPi0Tree->Branch("ph1_pos_y", &mPi0.ph1_pos_y, "ph1_pos_y/F");
    mPi0Tree->Branch("ph1_pos_z", &mPi0.ph1_pos_z, "ph1_pos_z/F");
    
    // Photon 2 branches
    mPi0Tree->Branch("ph2_energy", &mPi0.ph2_energy, "ph2_energy/F");
    mPi0Tree->Branch("ph2_eta", &mPi0.ph2_eta, "ph2_eta/F");
    mPi0Tree->Branch("ph2_phi", &mPi0.ph2_phi, "ph2_phi/F");
    mPi0Tree->Branch("ph2_pos_x", &mPi0.ph2_pos_x, "ph2_pos_x/F");
    mPi0Tree->Branch("ph2_pos_y", &mPi0.ph2_pos_y, "ph2_pos_y/F");
    mPi0Tree->Branch("ph2_pos_z", &mPi0.ph2_pos_z, "ph2_pos_z/F");
    
    cout << termcolor::green << "Initialization complete!" << termcolor::reset << endl;
    return kStOK;
}

//------------------------------------------------------
// Make: Process each event
//------------------------------------------------------
Int_t StPi0FinderForEEmc::Make()
{
    // Get MuDst Maker
    mMuDstMaker = (StMuDstMaker*)GetInputDS("MuDst");
    if (!mMuDstMaker) {
        if (mDebug > 0) {
            cerr << termcolor::red << "ERROR: StPi0FinderForEEmc::Make did not find StMuDstMaker" << termcolor::reset << endl;
        }
        return kStErr;
    }
    
    StMuEvent* muEvent = mMuDstMaker->muDst()->event();
    if (!muEvent) {
        if (mDebug > 0) {
            cerr << termcolor::red << "ERROR: StPi0FinderForEEmc::Make did not find MuDst event" << termcolor::reset << endl;
        }
        return kStErr;
    }
    
    mRunNumber = muEvent->runId();
    mEventNumber = muEvent->eventId();
    
    // Require energy maker (Part 1 calibration)
    if (!mEnergyMaker) {
        if (mDebug > 0) {
            cerr << termcolor::red << "ERROR: Energy maker not set (Part 1 calibration required)" << termcolor::reset << endl;
        }
        return kStErr;
    }
    
    // Require hit maker (Part 2 hit reconstruction)
    if (!mHitMaker) {
        if (mDebug > 0) {
            cerr << termcolor::red << "ERROR: Hit maker not set (Part 2 hit reconstruction required)" << termcolor::reset << endl;
        }
        return kStErr;
    }
    
    // Get vertex position for particle candidate reconstruction
    // Follow the same pattern as StEEmcTreeMaker
    const StThreeVectorF& vertexPos = muEvent->primaryVertexPosition();
    TVector3 vertex(vertexPos.x(), vertexPos.y(), vertexPos.z());
    
    // Find photon candidates from calibrated EEMC towers
    std::vector<EEmcParticleCandidate_t> photonCandidates;
    FindEEmcPhotons(photonCandidates, vertex);
    
    // Fill photon histograms
    for (Int_t iPh = 0; iPh < (Int_t)photonCandidates.size(); iPh++) {
        const EEmcParticleCandidate_t& photonCand = photonCandidates[iPh];
        
        h1_photon_energy->Fill(photonCand.E);
        h1_photon_eta->Fill(photonCand.momentum.Eta());
        h1_photon_phi->Fill(photonCand.momentum.Phi());
        h2_photon_energy_vs_eta->Fill(photonCand.momentum.Eta(), photonCand.E);
        
        // Use particle candidate position and momentum
        h2_photon_position->Fill(photonCand.position.X(), photonCand.position.Y());
        
        // Fill detector hitmap with all detected tower positions
        h2_detector_hitmap->Fill(photonCand.position.X(), photonCand.position.Y());
        h1_detector_phi->Fill(atan2(photonCand.position.Y(), photonCand.position.X()));
        
        if (mDebug > 2) {
            cout << termcolor::cyan << "    Photon " << iPh << ": eta=" << photonCand.momentum.Eta() << ", phi=" << photonCand.momentum.Phi()
                 << " -> position: x=" << photonCand.position.X() << " cm, y=" << photonCand.position.Y() << " cm"
                 << " [candidate PID=" << photonCand.PID << "]" << termcolor::reset << endl;
        }
    }
    
    // Pair photons to find pion candidates
    Int_t nPhotons = photonCandidates.size();
    for (Int_t i = 0; i < nPhotons; i++) {
        for (Int_t j = i + 1; j < nPhotons; j++) {
            const EEmcParticleCandidate_t& ph1Cand = photonCandidates[i];
            const EEmcParticleCandidate_t& ph2Cand = photonCandidates[j];
            
            // Extract 4-vectors from particle candidates
            TLorentzVector ph1;
            ph1.SetPxPyPzE(ph1Cand.momentum.x(), ph1Cand.momentum.y(), ph1Cand.momentum.z(), ph1Cand.E);
            TLorentzVector ph2;
            ph2.SetPxPyPzE(ph2Cand.momentum.x(), ph2Cand.momentum.y(), ph2Cand.momentum.z(), ph2Cand.E);
            
            // Calculate pi0 candidate properties
            Float_t e_total = ph1.Energy() + ph2.Energy();
            Float_t mass = CalculateInvariantMass(ph1, ph2);
            Float_t asymmetry = CalculateAsymmetry(ph1.Energy(), ph2.Energy());
            
            // Apply cuts
            if (e_total < mPi0EnergyMin) continue;
            if (mass > mPi0MassMax) continue;
            if (asymmetry > mAsymmetryMax) continue;
            
            // Combine 4-vectors
            TLorentzVector pi0 = ph1 + ph2;
            
            // Calculate opening angle
            TVector3 v1(ph1.Px(), ph1.Py(), ph1.Pz());
            TVector3 v2(ph2.Px(), ph2.Py(), ph2.Pz());
            Float_t openingAngle = v1.Angle(v2);
            
            // Calculate position on EEMC detector face from pi0 eta and phi
            Float_t z_eemc = 287.0;  // cm
            Float_t theta_pi0 = 2.0 * atan(exp(-pi0.Eta()));
            Float_t pos_x = z_eemc * tan(theta_pi0) * cos(pi0.Phi());
            Float_t pos_y = z_eemc * tan(theta_pi0) * sin(pi0.Phi());
            Float_t pos_z = z_eemc;  // EEMC z-position
            
            // Fill pi0 candidate structure
            mPi0.energy = e_total;
            mPi0.mass = mass;
            mPi0.pt = pi0.Pt();
            mPi0.eta = pi0.Eta();
            mPi0.phi = pi0.Phi();
            mPi0.openingAngle = openingAngle;
            mPi0.asymmetry = asymmetry;
            mPi0.pos_x = pos_x;
            mPi0.pos_y = pos_y;
            mPi0.pos_z = pos_z;
            
            // Photon 1
            mPi0.ph1_energy = ph1.Energy();
            mPi0.ph1_eta = ph1.Eta();
            mPi0.ph1_phi = ph1.Phi();
            Float_t z_eemc_1 = 287.0;
            Float_t theta_1 = 2.0 * atan(exp(-ph1.Eta()));
            mPi0.ph1_pos_x = z_eemc_1 * tan(theta_1) * cos(ph1.Phi());
            mPi0.ph1_pos_y = z_eemc_1 * tan(theta_1) * sin(ph1.Phi());
            mPi0.ph1_pos_z = z_eemc_1;
            
            // Photon 2
            mPi0.ph2_energy = ph2.Energy();
            mPi0.ph2_eta = ph2.Eta();
            mPi0.ph2_phi = ph2.Phi();
            Float_t z_eemc_2 = 287.0;
            Float_t theta_2 = 2.0 * atan(exp(-ph2.Eta()));
            mPi0.ph2_pos_x = z_eemc_2 * tan(theta_2) * cos(ph2.Phi());
            mPi0.ph2_pos_y = z_eemc_2 * tan(theta_2) * sin(ph2.Phi());
            mPi0.ph2_pos_z = z_eemc_2;
            
            // Fill histograms
            h1_pi0_mass->Fill(mass);
            h1_pi0_energy->Fill(e_total);
            h1_pi0_pt->Fill(pi0.Pt());
            h1_pi0_eta->Fill(pi0.Eta());
            h1_pi0_opening_angle->Fill(openingAngle);
            h1_pi0_asymmetry->Fill(asymmetry);
            h2_pi0_mass_vs_energy->Fill(e_total, mass);
            h2_pi0_position->Fill(pos_x, pos_y);
            
            // Fill tree
            mPi0Tree->Fill();
        }
    }
    
    return kStOK;
}

//------------------------------------------------------
// Finish: Write output and cleanup
//------------------------------------------------------
Int_t StPi0FinderForEEmc::Finish()
{
    cout << termcolor::green << "========================================" << termcolor::reset << endl;
    cout << termcolor::green << "Finishing StPi0FinderForEEmc" << termcolor::reset << endl;
    cout << termcolor::green << "========================================" << termcolor::reset << endl;
    
    // Write histograms
    if (mHistogramOutput) {
        mHistogramOutput->cd();
        h1_photon_energy->Write();
        h1_photon_eta->Write();
        h1_photon_phi->Write();
        h2_photon_energy_vs_eta->Write();
        h2_photon_position->Write();
        
        h1_pi0_mass->Write();
        h1_pi0_energy->Write();
        h1_pi0_pt->Write();
        h1_pi0_eta->Write();
        h1_pi0_opening_angle->Write();
        h1_pi0_asymmetry->Write();
        h2_pi0_mass_vs_energy->Write();
        h2_pi0_position->Write();
        h2_detector_hitmap->Write();
        h1_detector_phi->Write();
        
        cout << termcolor::green << "Histograms written to: " << mHistogramFileName << termcolor::reset << endl;
        mHistogramOutput->Close();
    }
    
    // Write tree
    if (mTreeOutput) {
        mTreeOutput->cd();
        if (mPi0Tree) {
            cout << termcolor::green << "Total pi0 candidates found: " << mPi0Tree->GetEntries() << termcolor::reset << endl;
            mPi0Tree->Write();
        }
        cout << termcolor::green << "Tree written to: " << mTreeFileName << termcolor::reset << endl;
        mTreeOutput->Close();
    }
    
    return kStOK;
}

//------------------------------------------------------
// Helper Methods
//------------------------------------------------------

void StPi0FinderForEEmc::FindEEmcPhotons(std::vector<EEmcParticleCandidate_t>& candidates,
                                         TVector3& vertex)
{
    if (mDebug > 0) {
        cout << termcolor::green << "FindEEmcPhotons: Extracting EEMC hits from Hit Maker (Part 2)" << termcolor::reset << endl;
    }
    
    // Get hit array from the hit maker (Part 2 reconstruction)
    // This provides SMD-clustered hits with improved position resolution
    const StEEmcHitVec_t& hitVec = mHitMaker->getHitVec();
    
    if (mDebug > 1) {
        cout << termcolor::green << "FindEEmcPhotons: Found " << hitVec.size() << " hits from StEEmcHitMaker" << termcolor::reset << endl;
    }

    if (hitVec.empty()) {
        if (mDebug > 0) {
            cout << termcolor::yellow << "FindEEmcPhotons: No hits in hit vector, skipping event" << termcolor::reset << endl;
        }
        return;
    }

    // Get calibrated EEMC energies from Part 1 energy maker (for quality checks)
    const EEmcEnergy_t* eemcEnergy = mEnergyMaker->getEEmcEnergyPtr();
    if (!eemcEnergy) {
        if (mDebug > 0) {
            cout << termcolor::red << "ERROR: No EEMC energy structure from energy maker" << termcolor::reset << endl;
        }
        return;
    }
    
    // Iterate through all hits and create particle candidates
    for (UInt_t iHit = 0; iHit < hitVec.size(); ++iHit) {
        const StEEmcHit_t& hit = hitVec[iHit];
        
        // Create EEmcHit_t structure for particle candidate from the StEEmcHit_t
        // following the pattern from StEEmcTreeMaker's copyStEEmcHitToEEmcHit
        EEmcHit_t candidateHit;
        candidateHit.x = hit.getPosition().X();
        candidateHit.y = hit.getPosition().Y();
        candidateHit.eta = hit.getPosition().Eta();
        candidateHit.phi = hit.getPosition().Phi();
        candidateHit.centralTowerIdx = hit.getTowerIdx();
        
        // Calculate total tower energy from the hit's tower indices and weights
        // Use public accessor methods since array members are protected
        const ETowEnergy_t& eTow = eemcEnergy->eTow;
        
        Int_t numUsedTowers = hit.getNumUsedTowers();
        candidateHit.numUsedTowers = numUsedTowers;
        if (candidateHit.numUsedTowers > EEmcHit_t::kMaxNumTowers)
            candidateHit.numUsedTowers = EEmcHit_t::kMaxNumTowers;
        
        candidateHit.eTow = 0;
        
        for (Int_t iTow = 0; iTow < candidateHit.numUsedTowers; ++iTow) {
            Int_t idx = hit.getUseTowerIndex(iTow);
            Float_t weight = hit.getUseTowerWeight(iTow);
            const EEmcElement_t& elem = eTow.getByIdx(idx);
            
            candidateHit.usedTowerIdx[iTow] = idx;
            candidateHit.usedTowerWeight[iTow] = weight;
            candidateHit.eTow += (elem.fail ? 0 : elem.energy) * weight;
        }
        
        Float_t energy = candidateHit.eTow;
        
        // Skip below energy threshold
        if (energy <= mPhotonEnergyMin) continue;
        
        // Skip negative energies
        if (energy < 0) continue;
        
        if (mDebug > 2) {
            cout << termcolor::green << "    Hit " << iHit << ": E=" << energy << " GeV"
                 << " -> position: x=" << candidateHit.x << " cm, y=" << candidateHit.y << " cm"
                 << " [tower idx=" << candidateHit.centralTowerIdx << "]" << termcolor::reset << endl;
        }
        
        // Create particle candidate using the standard constructor
        // This ensures momentum is computed from vertex properly
        // Use hit index as the ID
        EEmcParticleCandidate_t candidate(iHit, candidateHit, vertex);
        candidates.push_back(candidate);
    }
    
    if (mDebug > 1) {
        cout << termcolor::green << "FindEEmcPhotons: Created " << candidates.size() 
             << " particle candidates from hits above " << mPhotonEnergyMin << " GeV" << termcolor::reset << endl;
    }
    
    if (mDebug > 0) {
        cout << termcolor::green << "FindEEmcPhotons: Complete" << termcolor::reset << endl;
    }
}

//------------------------------------------------------
// SetEnergyMaker: Set the EEMC Part 1 Energy Maker
//------------------------------------------------------
void StPi0FinderForEEmc::SetEnergyMaker(StEEmcEnergyMaker_t* energyMaker)
{
    mEnergyMaker = energyMaker;
    if (mDebug > 0) {
        cout << termcolor::green << "SetEnergyMaker: Energy maker set" << termcolor::reset << endl;
    }
}

//------------------------------------------------------
// SetHitMaker: Set the EEMC Part 2 Hit Maker
//------------------------------------------------------
void StPi0FinderForEEmc::SetHitMaker(StEEmcHitMakerSimple_t* hitMaker)
{
    mHitMaker = hitMaker;
    if (mDebug > 0) {
        cout << termcolor::green << "SetHitMaker: Hit maker set" << termcolor::reset << endl;
    }
}

Float_t StPi0FinderForEEmc::CalculateInvariantMass(const TLorentzVector& p1, const TLorentzVector& p2)
{
    // Use the helper class for consistent calculations
    return EEmcPi0Kinematics::InvariantMass(p1, p2);
}

Float_t StPi0FinderForEEmc::CalculateAsymmetry(Float_t e1, Float_t e2)
{
    // Use the helper class for consistent calculations
    return EEmcPi0Kinematics::Asymmetry(e1, e2);
}