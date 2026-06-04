#ifndef StPi0FinderForEEmc_def
#define StPi0FinderForEEmc_def

#include "StChain/StMaker.h"
#include "TString.h"
#include "TVector3.h"
#include "TLorentzVector.h"
#include <TTree.h>
#include <vector>
#include <cmath>

#include "StEvent/StTriggerId.h"
#include "StMuDSTMaker/COMMON/StMuTriggerIdCollection.h"
#include "StEEmcUtil/database/StEEmcDb.h"
#include "StEEmcPool/StEEmcTreeMaker/StEEmcEnergyMaker.h"
#include "StRoot/StEEmcPool/EEmcTreeContainers/EEmcParticleCandidate.h"
#include "/direct/star+u/bmagh001/MuDst/include/termcolor.hpp"

// Forward declarations
class StMuDstMaker;
class StEvent;
class TFile;
class TH1F;
class TH2F;
class TCanvas;
class StEEmcDbMaker;
class StEEmcDb;
class StEEmcEnergyMaker_t;
class StEEmcHitMakerSimple_t;

//======================================================
// Pi0 Candidate Structure
//======================================================
struct Pi0Candidate
{
    Float_t energy;          // Total energy of both photons
    Float_t mass;            // Invariant mass
    Float_t pt;              // Transverse momentum
    Float_t eta;             // Pseudorapidity
    Float_t phi;             // Azimuthal angle
    Float_t openingAngle;    // Opening angle between photons
    Float_t asymmetry;       // Energy asymmetry zgg
    Float_t pos_x;           // Position X
    Float_t pos_y;           // Position Y
    Float_t pos_z;           // Position Z
    
    // Photon 1 info
    Float_t ph1_energy;
    Float_t ph1_eta;
    Float_t ph1_phi;
    Float_t ph1_pos_x;
    Float_t ph1_pos_y;
    Float_t ph1_pos_z;
    
    // Photon 2 info
    Float_t ph2_energy;
    Float_t ph2_eta;
    Float_t ph2_phi;
    Float_t ph2_pos_x;
    Float_t ph2_pos_y;
    Float_t ph2_pos_z;
};

class StPi0FinderForEEmc : public StMaker
{
    private:
        StEvent* event;                              // StEvent pointer
        StMuDstMaker* mMuDstMaker;                   // MuDst maker
        StEEmcEnergyMaker_t* mEnergyMaker;          // EEMC energy maker (Part 1 calibration)
        StEEmcHitMakerSimple_t* mHitMaker;          // Hit maker (Part 2 hit reconstruction)
        
        TFile* mHistogramOutput;                     // Output file for histograms
        TFile* mTreeOutput;                          // Output file for trees
        TTree* mPi0Tree;                             // Tree to store pi0 candidate information
        
        // Output file and tree names
        TString mHistogramFileName;
        TString mTreeFileName;

        // Histograms for QA
        TH1F* h1_photon_energy;                      // Histogram for photon energy
        TH1F* h1_photon_eta;                         // Histogram for photon eta
        TH1F* h1_photon_phi;                         // Histogram for photon phi (azimuthal angle)
        TH1F* h1_pi0_mass;                           // Histogram for pi0 invariant mass
        TH1F* h1_pi0_pt;                             // Histogram for pi0 pt
        TH1F* h1_pi0_eta;                            // Histogram for pi0 eta
        TH1F* h1_pi0_energy;                         // Histogram for pi0 energy
        TH1F* h1_pi0_opening_angle;                  // Histogram for opening angle
        TH1F* h1_pi0_asymmetry;                      // Histogram for energy asymmetry
        TH2F* h2_photon_energy_vs_eta;              // 2D: photon energy vs eta
        TH2F* h2_photon_position;                   // 2D: photon position
        TH2F* h2_pi0_mass_vs_energy;                // 2D: pi0 mass vs energy
        TH2F* h2_pi0_position;                       // 2D: pi0 position
        TH2F* h2_detector_hitmap;                   // 2D: detector hit positions (all towers)
        TH1F* h1_detector_phi;                       // Histogram for detector hit phi (azimuthal angle)

        // Configuration
        Float_t mPhotonEnergyMin;                    // Minimum photon energy (GeV)
        Float_t mPi0EnergyMin;                       // Minimum pi0 energy (GeV)
        Float_t mPi0MassMax;                         // Maximum pi0 invariant mass (GeV)
        Float_t mAsymmetryMax;                       // Maximum energy asymmetry
        
        // Tree branches
        Pi0Candidate mPi0;
        Int_t mEventNumber;
        Int_t mRunNumber;

        int mDebug;                                  // Debug level

        // Helper methods
        void FindEEmcPhotons(std::vector<EEmcParticleCandidate_t>& candidates,
                            TVector3& vertex);
        Float_t CalculateInvariantMass(const TLorentzVector& p1, const TLorentzVector& p2);
        Float_t CalculateAsymmetry(Float_t e1, Float_t e2);

    public:
        StPi0FinderForEEmc(const char* name = "StPi0FinderForEEmc");
        virtual ~StPi0FinderForEEmc();

        virtual Int_t Init();
        virtual Int_t Make();
        virtual Int_t Finish();
        
        // Setters
        void SetHistogramFile(TString fileName) { mHistogramFileName = fileName; }
        void SetTreeFile(TString fileName) { mTreeFileName = fileName; }
        void SetPhotonEnergyMin(Float_t emin) { mPhotonEnergyMin = emin; }
        void SetPi0EnergyMin(Float_t emin) { mPi0EnergyMin = emin; }
        void SetPi0MassMax(Float_t mmax) { mPi0MassMax = mmax; }
        void SetAsymmetryMax(Float_t amax) { mAsymmetryMax = amax; }
        void SetDebugLevel(Int_t level) { mDebug = level; }
        void SetEnergyMaker(StEEmcEnergyMaker_t* energyMaker);
        void SetHitMaker(StEEmcHitMakerSimple_t* hitMaker);
        ClassDef(StPi0FinderForEEmc, 1)
};

#endif