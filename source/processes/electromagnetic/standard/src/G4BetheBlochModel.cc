//
// ********************************************************************
// * DISCLAIMER                                                       *
// *                                                                  *
// * The following disclaimer summarizes all the specific disclaimers *
// * of contributors to this software. The specific disclaimers,which *
// * govern, are listed with their locations in:                      *
// *   http://cern.ch/geant4/license                                  *
// *                                                                  *
// * Neither the authors of this software system, nor their employing *
// * institutes,nor the agencies providing financial support for this *
// * work  make  any representation or  warranty, express or implied, *
// * regarding  this  software system or assume any liability for its *
// * use.                                                             *
// *                                                                  *
// * This  code  implementation is the  intellectual property  of the *
// * GEANT4 collaboration.                                            *
// * By copying,  distributing  or modifying the Program (or any work *
// * based  on  the Program)  you indicate  your  acceptance of  this *
// * statement, and all its terms.                                    *
// ********************************************************************
//
// $Id: G4BetheBlochModel.cc,v 1.2 2004/12/01 19:37:14 vnivanch Exp $
// GEANT4 tag $Name: geant4-07-00-cand-03 $
//
// -------------------------------------------------------------------
//
// GEANT4 Class header file
//
//
// File name:     G4BetheBlochModel
//
// Author:        Vladimir Ivanchenko on base of Laszlo Urban code
//
// Creation date: 03.01.2002
//
// Modifications:
//
// 04-12-02 Fix problem of G4DynamicParticle constructor (V.Ivanchenko)
// 23-12-02 Change interface in order to move to cut per region (V.Ivanchenko)
// 27-01-03 Make models region aware (V.Ivanchenko)
// 13-02-03 Add name (V.Ivanchenko)
//
// -------------------------------------------------------------------
//


//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

#include "G4BetheBlochModel.hh"
#include "Randomize.hh"
#include "G4Electron.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

using namespace std;

G4BetheBlochModel::G4BetheBlochModel(const G4ParticleDefinition* p, const G4String& nam)
  : G4VEmModel(nam),
  particle(0),
  highKinEnergy(100.*TeV),
  lowKinEnergy(2.0*MeV),
  twoln10(2.0*log(10.0)),
  bg2lim(0.0169),
  taulim(8.4146e-3),
  isIon(false)
{
  if(p) SetParticle(p);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4BetheBlochModel::~G4BetheBlochModel()
{}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4BetheBlochModel::SetParticle(const G4ParticleDefinition* p)
{
  if(particle != p) {
    particle = p;
    mass = particle->GetPDGMass();
    spin = particle->GetPDGSpin();
    G4double q = particle->GetPDGCharge()/eplus;
    chargeSquare = q*q;
    ratio = electron_mass_c2/mass;
    if(particle->GetParticleName() == "GenericIon") isIon = true;
  }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4double G4BetheBlochModel::HighEnergyLimit(const G4ParticleDefinition* p)
{
  if(!particle) SetParticle(p);
  return highKinEnergy;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4double G4BetheBlochModel::LowEnergyLimit(const G4ParticleDefinition* p)
{
  if(!particle) SetParticle(p);
  return lowKinEnergy;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4double G4BetheBlochModel::MinEnergyCut(const G4ParticleDefinition*,
                                         const G4MaterialCutsCouple* couple)
{
  return couple->GetMaterial()->GetIonisation()->GetMeanExcitationEnergy();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4bool G4BetheBlochModel::IsInCharge(const G4ParticleDefinition* p)
{
  if(!particle) SetParticle(p);
  return (p->GetPDGCharge() != 0.0 && p->GetPDGMass() > 10.*MeV);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4BetheBlochModel::Initialise(const G4ParticleDefinition* p,
                                   const G4DataVector&)
{
  if(!particle) SetParticle(p);
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4double G4BetheBlochModel::ComputeDEDX(const G4MaterialCutsCouple* couple,
                                        const G4ParticleDefinition* p,
                                              G4double kineticEnergy,
                                              G4double cut)
{
  G4double tmax      = MaxSecondaryEnergy(p, kineticEnergy);
  G4double cutEnergy = min(cut,tmax);

  G4double tau   = kineticEnergy/mass;
  G4double gam   = tau + 1.0;
  G4double bg2   = tau * (tau+2.0);
  G4double beta2 = bg2/(gam*gam);

  const G4Material* material = couple->GetMaterial();
  G4double eexc  = material->GetIonisation()->GetMeanExcitationEnergy();
  G4double eexc2 = eexc*eexc;
  G4double taul  = material->GetIonisation()->GetTaul();
  G4double cden  = material->GetIonisation()->GetCdensity();
  G4double mden  = material->GetIonisation()->GetMdensity();
  G4double aden  = material->GetIonisation()->GetAdensity();
  G4double x0den = material->GetIonisation()->GetX0density();
  G4double x1den = material->GetIonisation()->GetX1density();
  G4double* shellCorrectionVector =
            material->GetIonisation()->GetShellCorrectionVector();
  G4double eDensity = material->GetElectronDensity();

  G4double dedx = log(2.0*electron_mass_c2*bg2*cutEnergy/eexc2)-(1.0 + cutEnergy/tmax)*beta2;

  if(0.5 == spin) {
    G4double del = 0.5*cutEnergy/(kineticEnergy + mass);
    dedx += del*del;
  }

  // density correction
  G4double x = log(bg2)/twoln10;
  if ( x >= x0den ) {
    dedx -= twoln10*x - cden ;
    if ( x < x1den ) dedx -= aden*pow((x1den-x),mden) ;
  }

  // shell correction
  G4double sh = 0.0;
  x  = 1.0;

  if ( bg2 > bg2lim ) {
    for (G4int k=0; k<3; k++) {
	x *= bg2 ;
	sh += shellCorrectionVector[k]/x;
    }

  } else {
    for (G4int k=0; k<3; k++) {
	x *= bg2lim ;
	sh += shellCorrectionVector[k]/x;
    }
    sh *= log(tau/taul)/log(taulim/taul);
  }
  dedx -= sh;

  // now compute the total ionization loss

  if (dedx < 0.0) dedx = 0.0 ;

  dedx *= twopi_mc2_rcl2*chargeSquare*eDensity/beta2;

  return dedx;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4double G4BetheBlochModel::CrossSection(const G4MaterialCutsCouple* couple,
                                         const G4ParticleDefinition* p,
                                               G4double kineticEnergy,
                                               G4double cutEnergy,
                                               G4double maxKinEnergy)
{
  G4double cross = 0.0;
  G4double tmax = MaxSecondaryEnergy(p, kineticEnergy);
  G4double maxEnergy = min(tmax,maxKinEnergy);
  if(cutEnergy < maxEnergy) {

    G4double totEnergy = kineticEnergy + mass;
    G4double energy2   = totEnergy*totEnergy;
    G4double beta2     = kineticEnergy*(kineticEnergy + 2.0*mass)/energy2;

    cross = 1.0/cutEnergy - 1.0/maxEnergy - beta2*log(maxEnergy/cutEnergy)/tmax;

    // +term for spin=1/2 particle
    if( 0.5 == spin ) cross += 0.5*(maxEnergy - cutEnergy)/energy2;

    cross *= twopi_mc2_rcl2*chargeSquare*
             (couple->GetMaterial()->GetElectronDensity())/beta2;
  }
  //  G4cout << "BB: e= " << kineticEnergy << " tmin= " << cutEnergy << " tmax= " << tmax
  //       << " cross= " << cross << G4endl;
  return cross;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4DynamicParticle* G4BetheBlochModel::SampleSecondary(
                             const G4MaterialCutsCouple*,
                             const G4DynamicParticle* dp,
                                   G4double minEnergy,
                                   G4double maxEnergy)
{
  G4double tmax = MaxSecondaryEnergy(dp);
  G4double maxKinEnergy = min(maxEnergy,tmax);
  G4double minKinEnergy = min(minEnergy,maxKinEnergy);

  G4double kineticEnergy = dp->GetKineticEnergy();
  G4double totEnergy     = kineticEnergy + mass;
  G4double etot2         = totEnergy*totEnergy;
  G4double beta2         = kineticEnergy*(kineticEnergy + 2.0*mass)/etot2;

  G4double deltaKinEnergy, f;

  // sampling follows ...
  do {
    G4double q = G4UniformRand();
    deltaKinEnergy = minKinEnergy*maxKinEnergy/(minKinEnergy*(1.0 - q) + maxKinEnergy*q);


    f = 1.0 - beta2*deltaKinEnergy/tmax;
    if( 0.5 == spin ) f += 0.5*deltaKinEnergy*deltaKinEnergy/etot2;

    if(f > 1.0) {
        G4cout << "G4BetheBlochModel::SampleSecondary Warning! "
               << "Majorant 1.0 < "
               << f << " for Edelta= " << deltaKinEnergy
               << G4endl;
    }

  } while( G4UniformRand() > f );

  G4double totMomentum = totEnergy*sqrt(beta2);
  G4double deltaMomentum =
           sqrt(deltaKinEnergy * (deltaKinEnergy + 2.0*electron_mass_c2));
  G4double cost = deltaKinEnergy * (totEnergy + electron_mass_c2) /
                                   (deltaMomentum * totMomentum);
  G4double sint = sqrt(1.0 - cost*cost);

  G4double phi = twopi * G4UniformRand() ;


  G4ThreeVector deltaDirection(sint*cos(phi),sint*sin(phi), cost) ;
  G4ThreeVector direction = dp->GetMomentumDirection();
  deltaDirection.rotateUz(direction);

  // create G4DynamicParticle object for delta ray
  G4DynamicParticle* delta = new G4DynamicParticle(G4Electron::Electron(),
                                                   deltaDirection,deltaKinEnergy);

  return delta;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

vector<G4DynamicParticle*>* G4BetheBlochModel::SampleSecondaries(
                             const G4MaterialCutsCouple* couple,
                             const G4DynamicParticle* dp,
                                   G4double tmin,
                                   G4double maxEnergy)
{
  vector<G4DynamicParticle*>* vdp = new vector<G4DynamicParticle*>;
  G4DynamicParticle* delta = SampleSecondary(couple, dp, tmin, maxEnergy);
  vdp->push_back(delta);

  return vdp;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....
