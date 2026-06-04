#ifndef EEmcPhotonUtils_h
#define EEmcPhotonUtils_h

//==========================================================================
// EEmcPhotonUtils.h
// Helper utilities for EEMC photon finding and pion reconstruction
// Contains kinematics calculations and photon candidate structures
//==========================================================================

#include "TVector3.h"
#include "TLorentzVector.h"
#include <cmath>
#include <TMath.h>

//==========================================================================
// Photon Candidate Structure
// Stores basic photon cluster properties
//==========================================================================
struct EEmcPhoton
{
    Float_t energy;           // Cluster energy [GeV]
    TVector3 position;        // Cluster position in 3D space [cm]
    Int_t sector;             // EEMC sector (0-11)
    Int_t nTowers;            // Number of towers in cluster
    Float_t chi2;             // Fit quality
    
    EEmcPhoton() : energy(0), sector(-1), nTowers(0), chi2(-1) {}
    
    EEmcPhoton(Float_t e, const TVector3& pos) 
        : energy(e), position(pos), sector(-1), nTowers(0), chi2(-1) {}
    
    // Convert to 4-vector (assuming photon mass = 0)
    TLorentzVector Get4Vector() const
    {
        if (position.Mag() <= 0) {
            return TLorentzVector(0, 0, 0, 0);
        }
        
        TVector3 momentum = position.Unit() * energy;
        // Photon mass = 0, so E = pc
        return TLorentzVector(momentum.X(), momentum.Y(), momentum.Z(), energy);
    }
    
    Float_t Eta() const 
    {
        return position.Eta();
    }
    
    Float_t Phi() const
    {
        return position.Phi();
    }
    
    Float_t Pt() const
    {
        return position.Perp() * energy / position.Mag();
    }
};

//==========================================================================
// Pi0 Kinematics Calculator
// Static methods for calculating pi0 properties from two photons
//==========================================================================
class EEmcPi0Kinematics
{
public:
    //---------- Invariant Mass Calculations ----------
    
    // Method 1: Full 4-vector calculation (most accurate)
    static Float_t InvariantMass(const TLorentzVector& photon1, 
                                const TLorentzVector& photon2)
    {
        TLorentzVector pair = photon1 + photon2;
        return pair.M();
    }
    
    // Method 2: Using photon energies and opening angle
    // Formula: M = E * sin(theta/2) * sqrt(1 - zgg^2)
    // where zgg = |E1 - E2| / (E1 + E2)
    static Float_t InvariantMassOpeningAngle(Float_t e1, Float_t e2, 
                                            Float_t openingAngle)
    {
        Float_t e_total = e1 + e2;
        if (e_total <= 0) return -1.0;
        
        Float_t zgg = TMath::Abs(e1 - e2) / e_total;
        Float_t mass = e_total * TMath::Sin(openingAngle / 2.0) 
                      * TMath::Sqrt(1.0 - zgg * zgg);
        return mass;
    }
    
    //---------- Energy and Momentum Calculations ----------
    
    // Total energy of the pair
    static Float_t TotalEnergy(Float_t e1, Float_t e2)
    {
        return e1 + e2;
    }
    
    // Energy asymmetry: zgg = |E1 - E2| / (E1 + E2)
    static Float_t Asymmetry(Float_t e1, Float_t e2)
    {
        Float_t e_total = e1 + e2;
        if (e_total <= 0) return -1.0;
        return TMath::Abs(e1 - e2) / e_total;
    }
    
    // Opening angle between two photon directions
    static Float_t OpeningAngle(const TVector3& dir1, const TVector3& dir2)
    {
        if (dir1.Mag() <= 0 || dir2.Mag() <= 0) return -1.0;
        return dir1.Angle(dir2);
    }
    
    // Transverse momentum of pair
    static TLorentzVector Pair4Vector(const TLorentzVector& photon1, 
                                      const TLorentzVector& photon2)
    {
        return photon1 + photon2;
    }
    
    // Transverse momentum magnitude
    static Float_t TransverseMomentum(const TLorentzVector& pair)
    {
        return pair.Pt();
    }
    
    // Pseudorapidity
    static Float_t Pseudorapidity(const TLorentzVector& pair)
    {
        return pair.Eta();
    }
    
    // Azimuthal angle
    static Float_t AzimuthalAngle(const TLorentzVector& pair)
    {
        return pair.Phi();
    }
    
    //---------- Photon-specific Calculations ----------
    
    // Momentum from energy and position
    static TVector3 MomentumFromPhoton(Float_t energy, const TVector3& position)
    {
        if (position.Mag() <= 0) return TVector3(0, 0, 0);
        return position.Unit() * energy;
    }
    
    // Position weighted by energy (for pair position)
    static TVector3 WeightedPosition(const TVector3& pos1, Float_t e1,
                                    const TVector3& pos2, Float_t e2)
    {
        Float_t e_total = e1 + e2;
        if (e_total <= 0) return TVector3(0, 0, 0);
        return (pos1 * e1 + pos2 * e2) * (1.0 / e_total);
    }
    
    //---------- Standard Cuts ----------
    
    // Check if pair passes standard kinematic cuts
    static Bool_t PassesCuts(Float_t energy, Float_t mass, 
                           Float_t asymmetry,
                           Float_t eMin = 2.0,        // GeV
                           Float_t mMax = 1.0,        // GeV
                           Float_t aMax = 0.7)        // zgg
    {
        if (energy < eMin) return kFALSE;
        if (mass > mMax) return kFALSE;
        if (asymmetry > aMax) return kFALSE;
        return kTRUE;
    }
    
    //---------- Utility Helpers ----------
    
    // Get EEMC sector from azimuthal angle (0-11)
    static Int_t GetSectorFromPhi(Float_t phi)
    {
        Float_t phi_deg = phi * 180.0 / TMath::Pi();
        if (phi_deg < 0) phi_deg += 360.0;
        
        Int_t sector = (Int_t)(phi_deg / 30.0);
        if (sector >= 12) sector = 11;
        return sector;
    }
    
    // Convert sector number to center phi angle
    static Float_t SectorToPhi(Int_t sector)
    {
        if (sector < 0 || sector > 11) return 0;
        return (sector + 0.5) * 30.0 * TMath::Pi() / 180.0;
    }
};

#endif
