// This code implementation is the intellectual property of
// the RD44 GEANT4 collaboration.
//
// By copying, distributing or modifying the Program (or any work
// based on the Program) you indicate your acceptance of this statement,
// and all its terms.
//
// $Id: G4eEnergyLossPlus.cc,v 2.2 1998/12/09 09:15:15 urban Exp $
// GEANT4 tag $Name: geant4-00 $
//  
// $Id: 
// -----------------------------------------------------------
//      GEANT 4 class implementation file 
//
//      For information related to this code contact:
//      CERN, IT Division, ASD group
//      History: based on object model of
//      2nd December 1995, G.Cosmo
//      ---------- G4eEnergyLossPlus physics process -----------
//                by Laszlo Urban, 20 March 1997 
// **************************************************************
// It is the first implementation of the NEW UNIFIED ENERGY LOSS PROCESS.
// It calculates the energy loss of e+/e-.
// --------------------------------------------------------------
// 18/11/98  , L. Urban
//  It is a modified version of G4eEnergyLoss:
//  continuous energy loss with generation of subcutoff delta rays
// --------------------------------------------------------------
 
#include "G4eEnergyLossPlus.hh"
#include "G4EnergyLossTables.hh"
#include "G4EnergyLossMessenger.hh"

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

// Initialisation of static data members
// -------------------------------------
// Contributing processes : ion.loss + soft brems->NbOfProcesses is initialized
// to 2 . YOU DO NOT HAVE TO CHANGE this variable for a 'normal' run.
//
// You have to change NbOfProcesses if you invent a new process contributing
// to the continuous energy loss.
// The NbOfProcesses data member can be changed using the (public static)
// functions Get/Set/Plus/MinusNbOfProcesses (see G4eEnergyLossPlus.hh)

G4int            G4eEnergyLossPlus::NbOfProcesses = 2;

G4int            G4eEnergyLossPlus::CounterOfElectronProcess = 0;
G4int            G4eEnergyLossPlus::CounterOfPositronProcess = 0;
G4PhysicsTable** G4eEnergyLossPlus::RecorderOfElectronProcess =
                                           new G4PhysicsTable*[10];
G4PhysicsTable** G4eEnergyLossPlus::RecorderOfPositronProcess =
                                           new G4PhysicsTable*[10];
                                           
G4bool           G4eEnergyLossPlus::rndmStepFlag   = false;
G4bool           G4eEnergyLossPlus::EnlossFlucFlag = true;
G4double         G4eEnergyLossPlus::dRoverRange    = 20*perCent;
G4double         G4eEnergyLossPlus::finalRange     = 200*micrometer;                                           
G4double         G4eEnergyLossPlus::MinDeltaEnergy = 5.*keV ;

G4PhysicsTable*  G4eEnergyLossPlus::theDEDXElectronTable         = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theDEDXPositronTable         = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theRangeElectronTable        = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theRangePositronTable        = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theInverseRangeElectronTable = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theInverseRangePositronTable = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theLabTimeElectronTable      = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theLabTimePositronTable      = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theProperTimeElectronTable   = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theProperTimePositronTable   = NULL;

G4PhysicsTable*  G4eEnergyLossPlus::theeRangeCoeffATable         = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theeRangeCoeffBTable         = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::theeRangeCoeffCTable         = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::thepRangeCoeffATable         = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::thepRangeCoeffBTable         = NULL;
G4PhysicsTable*  G4eEnergyLossPlus::thepRangeCoeffCTable         = NULL;

G4EnergyLossMessenger* G4eEnergyLossPlus::eLossMessenger         = NULL;

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....
 
// constructor and destructor
 
G4eEnergyLossPlus::G4eEnergyLossPlus(const G4String& processName)
   : G4VContinuousDiscreteProcess (processName),
     theLossTable(NULL),
     theRangeCoeffATable(NULL),
     theRangeCoeffBTable(NULL),
     theRangeCoeffCTable(NULL),
     lastMaterial(NULL),                      
     LowestKineticEnergy(1.00*keV),
     HighestKineticEnergy(100.*TeV),
     MaxExcitationNumber (1.e6),
     probLimFluct (0.01),
     nmaxDirectFluct (100),
     nmaxCont1(4),
     nmaxCont2(16)
{
 //create (only once) EnergyLoss messenger 
 if(!eLossMessenger) eLossMessenger = new G4EnergyLossMessenger();
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4eEnergyLossPlus::~G4eEnergyLossPlus() 
{
     if (theLossTable) 
       {
         theLossTable->clearAndDestroy();
         delete theLossTable;
       }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo.... 

void G4eEnergyLossPlus::BuildDEDXTable(
                         const G4ParticleDefinition& aParticleType)
{
  ParticleMass = aParticleType.GetPDGMass(); 

  //  calculate data members TotBin,LOGRTable,RTable first

  G4double binning = 2.*dRoverRange;              //binning is 2.*dRoverRange
  G4double lrate = log(HighestKineticEnergy/LowestKineticEnergy);
  G4double nbin =  G4int((lrate/log(1.+binning) + lrate/log(1.+2.*binning))/2.);
  nbin = (nbin+50)/100; 
  TotBin =int(100*nbin) ;
  if (TotBin<100) TotBin = 100;
  if (TotBin>500) TotBin = 500;
  LOGRTable=lrate/TotBin;
  RTable   =exp(LOGRTable);

  // Build energy loss table as a sum of the energy loss due to the
  // different processes.                                           
  //

  const G4MaterialTable* theMaterialTable=G4Material::GetMaterialTable();
  G4int numOfMaterials = theMaterialTable->length();
  
  // create table for the total energy loss

  if (&aParticleType==G4Electron::Electron())
    {
      RecorderOfProcess=RecorderOfElectronProcess;
      CounterOfProcess=CounterOfElectronProcess;
      if (CounterOfProcess == NbOfProcesses)
        {
         if (theDEDXElectronTable)
           { 
             theDEDXElectronTable->clearAndDestroy();
             delete theDEDXElectronTable; 
           }
         theDEDXElectronTable = new G4PhysicsTable(numOfMaterials);
         theDEDXTable = theDEDXElectronTable;
        }
    }
  if (&aParticleType==G4Positron::Positron())
    {
     RecorderOfProcess=RecorderOfPositronProcess;
     CounterOfProcess=CounterOfPositronProcess;
     if (CounterOfProcess == NbOfProcesses)
       {
        if (theDEDXPositronTable)
          { 
            theDEDXPositronTable->clearAndDestroy();
            delete theDEDXPositronTable; 
          }
        theDEDXPositronTable = new G4PhysicsTable(numOfMaterials);
        theDEDXTable = theDEDXPositronTable;
       }
    }

  if (CounterOfProcess == NbOfProcesses)
    {
     // fill the tables
     // loop for materials
     G4double LowEdgeEnergy , Value;
     G4bool isOutRange;
     G4PhysicsTable* pointer;

     for (G4int J=0; J<numOfMaterials; J++)
        {
         // create physics vector and fill it

         G4PhysicsLogVector* aVector = new G4PhysicsLogVector(
                    LowestKineticEnergy, HighestKineticEnergy, TotBin);   

         // loop for the kinetic energy
   
         for (G4int i=0; i<TotBin; i++) 
            {
              LowEdgeEnergy = aVector->GetLowEdgeEnergy(i) ;      
              //here comes the sum of the different tables created by the  
              //processes (ionisation,bremsstrahlung,etc...)              
              Value = 0.;    
              for (G4int process=0; process < NbOfProcesses; process++)
                 {
                   pointer= RecorderOfProcess[process];
                   Value += (*pointer)[J]->GetValue(LowEdgeEnergy,isOutRange);
                 }

              aVector->PutValue(i,Value) ; 
            }

         theDEDXTable->insert(aVector) ;

        }

 
     //reset counter to zero
     if (&aParticleType==G4Electron::Electron()) CounterOfElectronProcess=0;
     if (&aParticleType==G4Positron::Positron()) CounterOfPositronProcess=0;

     // Build range table
     BuildRangeTable(aParticleType);  

     // Build lab/proper time tables
     BuildTimeTables(aParticleType);
 
     // Build coeff tables for the energy loss calculation

     BuildRangeCoeffATable(aParticleType);
     BuildRangeCoeffBTable(aParticleType);
     BuildRangeCoeffCTable(aParticleType);

     // invert the range table
     BuildInverseRangeTable(aParticleType);

     // make the energy loss and the range table available
     const G4double lowestKineticEnergy (1.00*keV);
     const G4double highestKineticEnergy(100.*TeV);
     G4EnergyLossTables::Register(&aParticleType,  
       (&aParticleType==G4Electron::Electron())?
       theDEDXElectronTable: theDEDXPositronTable,
       (&aParticleType==G4Electron::Electron())?
       theRangeElectronTable: theRangePositronTable,
       (&aParticleType==G4Electron::Electron())?
       theInverseRangeElectronTable: theInverseRangePositronTable,
       (&aParticleType==G4Electron::Electron())?
       theLabTimeElectronTable: theLabTimePositronTable,
       (&aParticleType==G4Electron::Electron())?
       theProperTimeElectronTable: theProperTimePositronTable,
       lowestKineticEnergy, highestKineticEnergy, 1.,TotBin);

    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....
      
void G4eEnergyLossPlus::BuildRangeTable(
                             const G4ParticleDefinition& aParticleType)
{                             
 // Build range table from the energy loss table
  
 const G4MaterialTable* theMaterialTable=G4Material::GetMaterialTable();
 G4int numOfMaterials = theMaterialTable->length();
 
 if (&aParticleType == G4Electron::Electron())
   {
     if (theRangeElectronTable)
       { theRangeElectronTable->clearAndDestroy();
         delete theRangeElectronTable;
       }
     theRangeElectronTable = new G4PhysicsTable(numOfMaterials);
     theRangeTable = theRangeElectronTable;
   }
 if (&aParticleType == G4Positron::Positron())
   {
     if (theRangePositronTable)
       { theRangePositronTable->clearAndDestroy();
         delete theRangePositronTable; 
       }
     theRangePositronTable = new G4PhysicsTable(numOfMaterials);
     theRangeTable = theRangePositronTable ;
   } 

 // loop for materials

 for (G4int J=0;  J<numOfMaterials; J++)
    {
      G4PhysicsLogVector* aVector = new G4PhysicsLogVector(LowestKineticEnergy,
                                                   HighestKineticEnergy,TotBin);
      BuildRangeVector(J, aVector);

      theRangeTable->insert(aVector);
    }
}    

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4eEnergyLossPlus::BuildTimeTables(
                             const G4ParticleDefinition& aParticleType)
{
 // Build time tables from the energy loss table
 
 const G4MaterialTable* theMaterialTable=G4Material::GetMaterialTable();
 G4int numOfMaterials = theMaterialTable->length();
 
 if (&aParticleType == G4Electron::Electron())
   {
     if (theLabTimeElectronTable)
       { theLabTimeElectronTable->clearAndDestroy();
         delete theLabTimeElectronTable; 
       }
     theLabTimeElectronTable = new G4PhysicsTable(numOfMaterials);
     theLabTimeTable = theLabTimeElectronTable;

     if (theProperTimeElectronTable)
       { theProperTimeElectronTable->clearAndDestroy();
         delete theProperTimeElectronTable;
       }
     theProperTimeElectronTable = new G4PhysicsTable(numOfMaterials);
     theProperTimeTable = theProperTimeElectronTable ;
   }
 if (&aParticleType == G4Positron::Positron())
   {
     if (theLabTimePositronTable)
       { theLabTimePositronTable->clearAndDestroy();
         delete theLabTimePositronTable;
       }
     theLabTimePositronTable = new G4PhysicsTable(numOfMaterials);
     theLabTimeTable = theLabTimePositronTable ;

     if (theProperTimePositronTable)
       { theProperTimePositronTable->clearAndDestroy();
         delete theProperTimePositronTable; 
       }
     theProperTimePositronTable = new G4PhysicsTable(numOfMaterials);
     theProperTimeTable = theProperTimePositronTable ;
   }

 // loop for materials

 for (G4int J=0;  J<numOfMaterials; J++)
    {
      G4PhysicsLogVector* aVector = new G4PhysicsLogVector(LowestKineticEnergy,
                                                  HighestKineticEnergy,TotBin);
      BuildLabTimeVector(J, aVector);

      theLabTimeTable->insert(aVector);
 

      G4PhysicsLogVector* bVector = new G4PhysicsLogVector(LowestKineticEnergy,
                                                   HighestKineticEnergy,TotBin);
      BuildProperTimeVector(J, bVector);

      theProperTimeTable->insert(bVector);
    }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4eEnergyLossPlus::BuildRangeVector(G4int materialIndex,
                                     G4PhysicsLogVector* rangeVector)
{                                     
  //  create range vector for a material
  G4int maxbint=100;
  G4bool isOut;
  G4double tlim=10.*keV,factor=2.*electron_mass_c2 ;

  G4PhysicsVector* physicsVector= (*theDEDXTable)[materialIndex];

  // low energy part first...
  G4double losslim = physicsVector->GetValue(tlim,isOut);
  G4double taulim  = tlim/electron_mass_c2;
  G4double clim    = losslim/sqrt(taulim);
  G4double ltaulim = log(taulim);
  G4double ltaumax = log(HighestKineticEnergy/electron_mass_c2);

  G4int i=-1;
  G4double Value, oldValue(0.);
  G4double LowEdgeEnergy, rangelim;
  G4double tau,tauold;

  do
    {
     i += 1 ;
     LowEdgeEnergy = rangeVector->GetLowEdgeEnergy(i);
     tau = LowEdgeEnergy/electron_mass_c2;
     if (tau <= taulim) Value = factor*sqrt(tau)/clim;
     else {
           rangelim = factor*taulim/losslim ;
           ltaulow  = log(taulim);
           ltauhigh = log(tau);
           Value    = rangelim+RangeIntLog(physicsVector,maxbint);
          }
     rangeVector->PutValue(i,Value);
     oldValue = Value;
     tauold   = tau;
 
    } while (tau<=taulim);

  i += 1;

  for (G4int j=i; j<TotBin; j++)
     {
      LowEdgeEnergy = rangeVector->GetLowEdgeEnergy(j);
      tau      = LowEdgeEnergy/electron_mass_c2;
      ltaulow  = log(tauold);
      ltauhigh = log(tau);
      Value    = oldValue+RangeIntLog(physicsVector,maxbint);
      rangeVector->PutValue(j,Value);
      oldValue = Value;
      tauold   = tau;
     }
}    

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4eEnergyLossPlus::BuildLabTimeVector(G4int materialIndex,
                                     G4PhysicsLogVector* timeVector)
//  create lab time vector for a material
{
  G4int maxbint=100;
  G4bool isOut;
  G4double tlim=5.*keV,parlowen=0.4,ppar=0.5-parlowen ;

  G4PhysicsVector* physicsVector= (*theDEDXTable)[materialIndex];

  // low energy part first...
  G4double losslim = physicsVector->GetValue(tlim,isOut);
  G4double taulim  = tlim/ParticleMass ;
  G4double clim    = sqrt(ParticleMass*tlim/2.)/(c_light*losslim*ppar);  
  G4double ltaulim = log(taulim);
  G4double ltaumax = log(HighestKineticEnergy/ParticleMass) ;

  G4int i=-1;
  G4double Value, oldValue(0.);
  G4double LowEdgeEnergy, timelim;
  G4double tau,tauold;

  do                               
    {
     i += 1 ;
     LowEdgeEnergy = timeVector->GetLowEdgeEnergy(i);
     tau = LowEdgeEnergy/ParticleMass;
     if (tau <= taulim) Value = clim*exp(ppar*log(tau/taulim));
     else {
           timelim  = clim;
           ltaulow  = log(taulim);
           ltauhigh = log(tau);
           Value    = timelim+LabTimeIntLog(physicsVector,maxbint);
          } 
     timeVector->PutValue(i,Value);
     oldValue = Value;
     tauold   = tau;
  
    } while (tau<=taulim) ;

  i += 1 ;
  
  for (G4int j=i; j<TotBin; j++)
     {
      LowEdgeEnergy = timeVector->GetLowEdgeEnergy(j);
      tau      = LowEdgeEnergy/ParticleMass;
      ltaulow  = log(tauold);
      ltauhigh = log(tau);
      Value    = oldValue+LabTimeIntLog(physicsVector,maxbint);
      timeVector->PutValue(j,Value);
      oldValue = Value ;
      tauold   = tau ;
     }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4eEnergyLossPlus::BuildProperTimeVector(G4int materialIndex,
                                     G4PhysicsLogVector* timeVector)
{
  //  create lab time vector for a material
  G4int maxbint=100;
  G4bool isOut;
  G4double tlim=5.*keV,parlowen=0.4,ppar=0.5-parlowen ;

  G4PhysicsVector* physicsVector= (*theDEDXTable)[materialIndex];

  // low energy part first...
  G4double losslim = physicsVector->GetValue(tlim,isOut);
  G4double taulim  = tlim/ParticleMass;
  G4double clim    = sqrt(ParticleMass*tlim/2.)/(c_light*losslim*ppar);  
  G4double ltaulim = log(taulim);
  G4double ltaumax = log(HighestKineticEnergy/ParticleMass);

  G4int i=-1;
  G4double Value, oldValue(0.);
  G4double LowEdgeEnergy, timelim;
  G4double tau,tauold;
  
  do                               
    {
     i += 1 ;
     LowEdgeEnergy = timeVector->GetLowEdgeEnergy(i);
     tau = LowEdgeEnergy/ParticleMass ;
     if (tau <= taulim) Value = clim*exp(ppar*log(tau/taulim));
     else {
           timelim  = clim;
           ltaulow  = log(taulim);
           ltauhigh = log(tau);
           Value    = timelim+ProperTimeIntLog(physicsVector,maxbint);
          } 
      timeVector->PutValue(i,Value);
      oldValue = Value;
      tauold   = tau;
  
    } while (tau<=taulim) ;

  i += 1 ;
  
  for (G4int j=i; j<TotBin; j++)
     {
      LowEdgeEnergy = timeVector->GetLowEdgeEnergy(j);
      tau      = LowEdgeEnergy/ParticleMass;
      ltaulow  = log(tauold);
      ltauhigh = log(tau);
      Value    = oldValue+ProperTimeIntLog(physicsVector,maxbint);
      timeVector->PutValue(j,Value);
      oldValue = Value;
      tauold   = tau;
     }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4double G4eEnergyLossPlus::RangeIntLog(G4PhysicsVector* physicsVector,
                                    G4int nbin)
//  num. integration, logarithmic binning
{
  G4double taui,lossi,ci;
  G4bool   isOut;

  G4double ltt   = ltauhigh-ltaulow;
  G4double dltau = ltt/nbin;
  G4double Value = 0.;

  for (G4int i=0; i<=nbin; i++)
     {
       taui  = exp(ltaulow+dltau*i);
       lossi = physicsVector->GetValue(ParticleMass*taui,isOut);
       if ((i==0)||(i==nbin)) ci=0.5; else ci=1.;
       Value += ci*taui/lossi;
     }

  return Value*ParticleMass*dltau;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4double G4eEnergyLossPlus::LabTimeIntLog(G4PhysicsVector* physicsVector,
                                    G4int nbin)
//  num. integration, logarithmic binning
{
  G4double taui,ti,lossi,ci;
  G4bool   isOut;

  G4double ltt   = ltauhigh-ltaulow;
  G4double dltau = ltt/nbin;
  G4double Value = 0.;

  for (G4int i=0; i<=nbin; i++)
     {
       taui  = exp(ltaulow+dltau*i);
       ti    = ParticleMass*taui;
       lossi = physicsVector->GetValue(ti,isOut);
       if ((i==0)||(i==nbin)) ci=0.5; else ci=1.;
       Value += ci*taui*(ti+ParticleMass)/(sqrt(ti*(ti+2.*ParticleMass))*lossi);
     }

  return Value*ParticleMass*dltau/c_light;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4double G4eEnergyLossPlus::ProperTimeIntLog(G4PhysicsVector* physicsVector,
                                    G4int nbin)
//  num. integration, logarithmic binning
{
  G4double taui,ti,lossi,ci;
  G4bool   isOut;

  G4double ltt   = ltauhigh-ltaulow;
  G4double dltau = ltt/nbin;
  G4double Value = 0.;

  for (G4int i=0; i<=nbin; i++)
     {
       taui  = exp(ltaulow+dltau*i);
       ti    = ParticleMass*taui;
       lossi = physicsVector->GetValue(ti,isOut);
       if ((i==0)||(i==nbin)) ci=0.5; else ci=1.;
       Value += ci*taui*ParticleMass/(sqrt(ti*(ti+2.*ParticleMass))*lossi);
     }

  return Value*ParticleMass*dltau/c_light;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4eEnergyLossPlus::BuildRangeCoeffATable(
                            const G4ParticleDefinition& aParticleType)
{
  // Build tables of coefficients for the energy loss calculation
  // create table for coefficients "A"

  const G4MaterialTable* theMaterialTable = G4Material::GetMaterialTable();
  G4int numOfMaterials = theMaterialTable->length();

  if (&aParticleType==G4Electron::Electron())
    {
      if (theeRangeCoeffATable) {theeRangeCoeffATable->clearAndDestroy();
                                 delete theeRangeCoeffATable;
                                }
      theeRangeCoeffATable = new G4PhysicsTable(numOfMaterials);
      theRangeCoeffATable  = theeRangeCoeffATable ;
    }
  if (&aParticleType==G4Positron::Positron())
    {
      if (thepRangeCoeffATable) {thepRangeCoeffATable->clearAndDestroy();
                                 delete thepRangeCoeffATable; 
                                }
      thepRangeCoeffATable = new G4PhysicsTable(numOfMaterials);
      theRangeCoeffATable  = thepRangeCoeffATable;
    }
 
  G4double R1 = RTable+1., R2 = RTable*RTable ;
  G4double w = R1*(RTable-1.)*(RTable-1.);
  G4double w1 = RTable/w , w2 = -RTable*R1/w , w3 = R2/w ;
  G4double Ti , Tim , Tip , Ri , Rim , Rip , Value;
  G4bool isOut;

  // loop for materials

  for (G4int J=0; J<numOfMaterials; J++)
     {
      G4PhysicsLinearVector* aVector = new G4PhysicsLinearVector(0.,TotBin,TotBin);

      // loop for kinetic energy   
      G4PhysicsVector* rangeVector= (*theRangeTable)[J];
      Ti = LowestKineticEnergy;
      
      for (G4int i=0; i<TotBin; i++)
         {
           Ri = rangeVector->GetValue(Ti,isOut);    
           if (i==0) Rim = Ri/sqrt(RTable);
           else { Tim = Ti/RTable; Rim = rangeVector->GetValue(Tim,isOut);}
           Tip = Ti*RTable;
           Rip = rangeVector->GetValue(Tip,isOut);
           if (i < (TotBin-1)) Value = (w1*Rip + w2*Ri + w3*Rim)/(Ti*Ti);
           else Value = 0.;
           aVector->PutValue(i,Value);
           Ti *= RTable;
         }
  
      theRangeCoeffATable->insert(aVector);
     } 
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4eEnergyLossPlus::BuildRangeCoeffBTable(
                            const G4ParticleDefinition& aParticleType)
{                            
 // Build tables of coefficients for the energy loss calculation
 // create table for coefficients "B"
 
    const G4MaterialTable* theMaterialTable = G4Material::GetMaterialTable();
    G4int numOfMaterials = theMaterialTable->length();

    if (&aParticleType==G4Electron::Electron())
      {
        if (theeRangeCoeffBTable) {theeRangeCoeffBTable->clearAndDestroy();
                                   delete theeRangeCoeffBTable;
                                  }
        theeRangeCoeffBTable = new G4PhysicsTable(numOfMaterials);
        theRangeCoeffBTable  = theeRangeCoeffBTable;
      }
    if (&aParticleType==G4Positron::Positron())
      {
        if (thepRangeCoeffBTable) {thepRangeCoeffBTable->clearAndDestroy();
                                   delete thepRangeCoeffBTable;
                                  }
        thepRangeCoeffBTable = new G4PhysicsTable(numOfMaterials);
        theRangeCoeffBTable  = thepRangeCoeffBTable;
  }

  G4double R1 = RTable+1., R2 = RTable*RTable;
  G4double w  = R1*(RTable-1.)*(RTable-1.);
  G4double w1 = -R1/w , w2 = R1*(R2+1.)/w , w3 = -R2*R1/w ;
  G4double Ti , Tim , Tip , Ri , Rim , Rip , Value ;
  G4bool isOut;

  //  loop for materials
  
  for (G4int J=0; J<numOfMaterials; J++)
     {
      G4PhysicsLinearVector* aVector = new G4PhysicsLinearVector(0.,TotBin,TotBin);

      // loop for kinetic energy
      G4PhysicsVector* rangeVector = (*theRangeTable)[J];
      Ti = LowestKineticEnergy;
   
      for ( G4int i=0; i<TotBin; i++)
         {
           Ri = rangeVector->GetValue(Ti,isOut);
           if (i==0) Rim = Ri/sqrt(RTable);
           else { Tim = Ti/RTable; Rim = rangeVector->GetValue(Tim,isOut);}
           Tip = Ti*RTable;
           Rip = rangeVector->GetValue(Tip,isOut);
           if (i < (TotBin-1)) Value = (w1*Rip + w2*Ri + w3*Rim)/Ti;  
           else                Value = RTable*(Ri-Rim)/((RTable-1.)*Ti);
           aVector->PutValue(i,Value);
           Ti *= RTable;
         }
  
      theRangeCoeffBTable->insert(aVector);
    } 
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4eEnergyLossPlus::BuildRangeCoeffCTable(
                            const G4ParticleDefinition& aParticleType)
{                            
// Build tables of coefficients for the energy loss calculation
// create table for coefficients "C"

   const G4MaterialTable* theMaterialTable = G4Material::GetMaterialTable();
   G4int numOfMaterials = theMaterialTable->length();

   if (&aParticleType==G4Electron::Electron())
     {
      if (theeRangeCoeffCTable) {theeRangeCoeffCTable->clearAndDestroy();
                                 delete theeRangeCoeffCTable;
                                }
      theeRangeCoeffCTable = new G4PhysicsTable(numOfMaterials);
      theRangeCoeffCTable  = theeRangeCoeffCTable;
     }
   if (&aParticleType==G4Positron::Positron())
     {
      if (thepRangeCoeffCTable) {thepRangeCoeffCTable->clearAndDestroy();
                                 delete thepRangeCoeffCTable;
                                }
      thepRangeCoeffCTable = new G4PhysicsTable(numOfMaterials);
      theRangeCoeffCTable  = thepRangeCoeffCTable ;
     }

  G4double R1 = RTable+1., R2 = RTable*RTable;
  G4double w = R1*(RTable-1.)*(RTable-1.);
  G4double w1 = 1./w , w2 = -RTable*R1/w , w3 = RTable*R2/w;
  G4double Ti , Tim , Tip , Ri , Rim , Rip , Value;
  G4bool isOut;

  // loop for materials
  for (G4int J=0; J<numOfMaterials; J++)
     {
      G4PhysicsLinearVector* aVector = new G4PhysicsLinearVector(0.,TotBin,TotBin);

      // loop for kinetic energy
      G4PhysicsVector* rangeVector = (*theRangeTable)[J];
      Ti = LowestKineticEnergy;
   
      for ( G4int i=0; i<TotBin; i++)
         {
           Ri = rangeVector->GetValue(Ti,isOut);    
           if (i==0) Rim = Ri/sqrt(RTable);
           else { Tim = Ti/RTable; Rim = rangeVector->GetValue(Tim,isOut);}
           Tip = Ti*RTable;
           Rip = rangeVector->GetValue(Tip,isOut);
           if (i < (TotBin-1)) Value = w1*Rip + w2*Ri + w3*Rim;
           else                Value = (-Ri+RTable*Rim)/(RTable-1.);
           aVector->PutValue(i,Value);
           Ti *= RTable;
         }
  
      theRangeCoeffCTable->insert(aVector);
     } 
}
 
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....
     
  void G4eEnergyLossPlus::BuildInverseRangeTable(
                             const G4ParticleDefinition& aParticleType)
{                             
 // Build inverse table of the range table

    G4double SmallestRange,BiggestRange;
    G4bool isOut;

 // create table

    const G4MaterialTable* theMaterialTable = G4Material::GetMaterialTable();
    G4int numOfMaterials = theMaterialTable->length();

    if (&aParticleType == G4Electron::Electron())
      {
       if (theInverseRangeElectronTable)
         {
           theInverseRangeElectronTable->clearAndDestroy();
           delete theInverseRangeElectronTable; 
         }
       theInverseRangeElectronTable = new G4PhysicsTable(numOfMaterials);
       theInverseRangeTable = theInverseRangeElectronTable;
       theRangeTable        = theRangeElectronTable;
       theDEDXTable         = theDEDXElectronTable;
       theRangeCoeffATable  = theeRangeCoeffATable;
       theRangeCoeffBTable  = theeRangeCoeffBTable;
       theRangeCoeffCTable  = theeRangeCoeffCTable;
      }
      
    if (&aParticleType == G4Positron::Positron())
      {
       if (theInverseRangePositronTable)
         {
           theInverseRangePositronTable->clearAndDestroy();
           delete theInverseRangePositronTable;
         }
       theInverseRangePositronTable = new G4PhysicsTable(numOfMaterials);
       theInverseRangeTable = theInverseRangePositronTable;
       theRangeTable        = theRangePositronTable;
       theDEDXTable         = theDEDXPositronTable;
       theRangeCoeffATable  = thepRangeCoeffATable;
       theRangeCoeffBTable  = thepRangeCoeffBTable;
       theRangeCoeffCTable  = thepRangeCoeffCTable;
      } 

    // loop for materials

    for (G4int J=0; J<numOfMaterials; J++)
       {
         SmallestRange = (*theRangeTable)(J)->GetValue(LowestKineticEnergy ,isOut);
         BiggestRange  = (*theRangeTable)(J)->GetValue(HighestKineticEnergy,isOut);

         G4PhysicsLogVector* aVector = new G4PhysicsLogVector(SmallestRange,
                                                              BiggestRange,TotBin);
         InvertRangeVector(J, aVector);

         theInverseRangeTable->insert(aVector);
       }
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

void G4eEnergyLossPlus::InvertRangeVector(G4int materialIndex,
                                      G4PhysicsLogVector* aVector)
{                                      
 //  invert range vector for a material

 G4double LowEdgeRange,A,B,C,discr,KineticEnergy;
 
 G4double Tbin = LowestKineticEnergy/RTable;
 G4double rangebin = 0.0; 
 G4int binnumber = -1;
 G4bool isOut;

 //loop for range values
 for (G4int i=0; i<TotBin; i++)
    {
      LowEdgeRange = aVector->GetLowEdgeEnergy(i);
      while ((rangebin < LowEdgeRange) && (binnumber < TotBin))  
           {
              binnumber += 1;
              Tbin *= RTable;
              rangebin = (*theRangeTable)(materialIndex)->GetValue(Tbin,isOut);
           }
   
      if      (binnumber == 0)        KineticEnergy = LowestKineticEnergy;
      else if (binnumber == TotBin-1) KineticEnergy = HighestKineticEnergy;
      else
          {
            A = (*(*theRangeCoeffATable)(materialIndex))(binnumber-1);
            B = (*(*theRangeCoeffBTable)(materialIndex))(binnumber-1);
            C = (*(*theRangeCoeffCTable)(materialIndex))(binnumber-1);
            if(A==0.)
              KineticEnergy = (LowEdgeRange -C )/B ;
            else
            {
              discr = B*B - 4.*A*(C-LowEdgeRange);
              discr = discr>0. ? sqrt(discr) : 0.;
              KineticEnergy = 0.5*(discr-B)/A ;
            }
          }

      aVector->PutValue(i,KineticEnergy) ;
    }
}
  
//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....
         
G4double G4eEnergyLossPlus::GetConstraints(const G4DynamicParticle* aParticle,
                                              G4Material* aMaterial) 
{
  // returns the Step limit 
  // dRoverRange is the max. allowed relative range loss in one Step
  // it calculates dEdx and the range as well....

  G4double CutInRange,StepLimit; 
  G4bool isOutRange;

  if (aParticle->GetDefinition()->GetPDGCharge() < 0.)
    {
      CutInRange = G4Electron::Electron()->GetCuts();
      theDEDXTable        = theDEDXElectronTable;
      theRangeTable       = theRangeElectronTable;
      theRangeCoeffATable = theeRangeCoeffATable;
      theRangeCoeffBTable = theeRangeCoeffBTable;
      theRangeCoeffCTable = theeRangeCoeffCTable;
    }
  else
    {
      CutInRange = G4Positron::Positron()->GetCuts();
      theDEDXTable        = theDEDXPositronTable;
      theRangeTable       = theRangePositronTable;
      theRangeCoeffATable = thepRangeCoeffATable;
      theRangeCoeffBTable = thepRangeCoeffBTable;
      theRangeCoeffCTable = thepRangeCoeffCTable;
    }

  G4double Thigh = HighestKineticEnergy/RTable;
  G4double KineticEnergy = aParticle->GetKineticEnergy();
  EnergyBinNumber = G4int(log(KineticEnergy/LowestKineticEnergy)/LOGRTable);
  
  G4double c1=dRoverRange , c2=2.*(1.-dRoverRange)*finalRange,
                 c3=-(1.-dRoverRange)*finalRange*finalRange;

  G4int index = aMaterial->GetIndex();
 
  if (KineticEnergy < LowestKineticEnergy)
    {
      // extrapolation for very low energy 
      fdEdx = sqrt(KineticEnergy/LowestKineticEnergy)*
             (*theDEDXTable)(index)->GetValue(LowestKineticEnergy,isOutRange);       
      fRangeNow = sqrt(KineticEnergy/LowestKineticEnergy)*
                  (*theRangeTable)(index)->GetValue(LowestKineticEnergy,isOutRange);
      StepLimit = fRangeNow;
    }
  else if ( KineticEnergy > Thigh)
    {
      // extrapolation for very high energy 
      fdEdx = (*theDEDXTable)(index)->GetValue(Thigh,isOutRange);
      fRangeNow = (*theRangeTable)(index)->GetValue(Thigh,isOutRange);
      if (fdEdx > 0.) fRangeNow += (KineticEnergy-Thigh)/fdEdx;
      StepLimit = c1*fRangeNow;
    }
  else
    {
      // LowestKineticEnergy <= KineticEnergy <= HighestKineticEnergy 
      fdEdx = (*theDEDXTable)(index)->GetValue(KineticEnergy,isOutRange);
      G4double RgCoefA = (*(*theRangeCoeffATable)(index))(EnergyBinNumber);
      G4double RgCoefB = (*(*theRangeCoeffBTable)(index))(EnergyBinNumber);
      G4double RgCoefC = (*(*theRangeCoeffCTable)(index))(EnergyBinNumber);
      fRangeNow = (RgCoefA*KineticEnergy+RgCoefB)*KineticEnergy+RgCoefC;         

      // compute the (random) Step limit
       if (fRangeNow>finalRange)
         {
           StepLimit = c1*fRangeNow+c2+c3/fRangeNow;
           //randomise this value
           if (rndmStepFlag) StepLimit = finalRange + (StepLimit-finalRange)*G4UniformRand();
           if (StepLimit > fRangeNow) StepLimit = fRangeNow;
         }
       else StepLimit = fRangeNow;
     }  

  return StepLimit; 
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4VParticleChange* G4eEnergyLossPlus::AlongStepDoIt( const G4Track& trackData,
                                                 const G4Step&  stepData)
{                              
 // compute the energy loss after a Step

  // get particle and material pointers from trackData 
  const G4DynamicParticle* aParticle = trackData.GetDynamicParticle();
  G4double E      = aParticle->GetKineticEnergy() ;
  G4double charge = aParticle->GetDefinition()->GetPDGCharge();
  
  G4Material* aMaterial = trackData.GetMaterial();
  G4int index = aMaterial->GetIndex();
  
  G4double Step = stepData.GetStepLength();
  
  aParticleChange.Initialize(trackData);  
  
  // do not track further if kin.energy < 1. eV
  const G4double MinKineticEnergy = 1.*eV;
   
  G4double MeanLoss, finalT; 
  
  if (E < MinKineticEnergy)   { finalT = 0.; MeanLoss = E;}
  
  else if (EnergyBinNumber <= 0)   
     {
       if (Step >= fRangeNow) { finalT = 0.; MeanLoss = E;}
       else
         {
           finalT = E*(1.-Step/fRangeNow)*(1.-Step/fRangeNow);
           if (finalT < MinKineticEnergy) finalT = 0.;
           MeanLoss = E - finalT;
         }
     }
    
  else if (EnergyBinNumber >= (TotBin-1))
     {
       // simple solution for the moment: loss = Step*dE/dx (dE/dx const)
       MeanLoss = Step*fdEdx;
       if (MeanLoss > E) MeanLoss = E;
       finalT = E - MeanLoss;
       if (finalT < MinKineticEnergy) { finalT = 0.; MeanLoss = E;}  
     }
     
  else if (Step >= fRangeNow) { finalT = 0.; MeanLoss = E;}
  
  else
     {
       // loss calculation with quadratic interpolation in the table
       if (charge<0.) finalT = G4EnergyLossTables::GetPreciseEnergyFromRange
                              (G4Electron::Electron(),fRangeNow-Step,aMaterial);
       else           finalT = G4EnergyLossTables::GetPreciseEnergyFromRange
                              (G4Positron::Positron(),fRangeNow-Step,aMaterial);
       if (finalT < MinKineticEnergy) finalT = 0.;
       MeanLoss = E-finalT;

       // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!
       // G4bool print = true ;
       G4bool print = false;
       if(MeanLoss > 0.)
       {
         G4double rcut,Tc,T0,presafety,postsafety,
                  delta,fragment ;
         G4double frperstep,x1,y1,z1,dx,dy,dz,dTime,time0,DeltaTime;
         if(charge < 0.)
         {
           rcut=G4Electron::Electron()->GetCuts();
           Tc=G4Electron::Electron()->GetCutsInEnergy()[index];
           // threshold !
           if(Tc > 0.5*E) Tc=0.5*E ;
         }
         else
         {
           rcut=G4Positron::Positron()->GetCuts();
           Tc=G4Positron::Positron()->GetCutsInEnergy()[index];
           // threshold !
           if(Tc > E) Tc=E ;
         }
         // generate subcutoff delta rays only if Tc>MinDeltaEnergy!
         if(Tc > MinDeltaEnergy)
         {
           presafety  = stepData.GetPreStepPoint()->GetSafety() ;
           postsafety = stepData.GetPostStepPoint()->GetSafety() ;

 // safety by hand for a layer (in z)
  //  presafety = min(
  //              abs(stepData.GetPreStepPoint()->GetPosition().z()-0.265),
  //              abs(stepData.GetPreStepPoint()->GetPosition().z()-0.265)); 
  //  postsafety= min(
  //              abs(stepData.GetPostStepPoint()->GetPosition().z()-0.265),
  //              abs(stepData.GetPostStepPoint()->GetPosition().z()-0.265)); 
        
           if((presafety>=rcut)&&(postsafety>=rcut))
           { 
             fragment = 0. ;
           }  
           else
           {
             x1=stepData.GetPreStepPoint()->GetPosition().x();
             y1=stepData.GetPreStepPoint()->GetPosition().y();
             z1=stepData.GetPreStepPoint()->GetPosition().z();
             dx=stepData.GetPostStepPoint()->GetPosition().x()-x1 ;
             dy=stepData.GetPostStepPoint()->GetPosition().y()-y1 ;
             dz=stepData.GetPostStepPoint()->GetPosition().z()-z1 ;
             time0=stepData.GetPreStepPoint()->GetGlobalTime();
             dTime=stepData.GetPostStepPoint()->GetGlobalTime()-time0;
           
             if((presafety<rcut)&&(postsafety<rcut))
             {
               fragment = Step ;
               frperstep=1. ;
             }
             else if(presafety<rcut)
             {
               delta=presafety*Step/(postsafety-presafety) ;
               fragment=rcut*(Step+delta)/postsafety-delta ;
               frperstep=fragment/Step;
             }
             else if(postsafety<rcut)
             {
               delta=postsafety*Step/(presafety-postsafety) ;
               fragment=rcut*(Step+delta)/presafety-delta ;
               x1 += dx;
               y1 += dy;
               z1 += dz;   
               time0 += dTime ;
               frperstep=-fragment/Step;
             }
           }

           if(fragment>0.)
           {
           
             if(charge<0.) T0=G4EnergyLossTables::GetPreciseEnergyFromRange(
                                                  G4Electron::Electron(),
                                                  min(presafety,postsafety),
                                                  aMaterial) ;           
             else          T0=G4EnergyLossTables::GetPreciseEnergyFromRange(
                                                 G4Positron::Positron(),
                                                 min(presafety,postsafety),
                                                 aMaterial) ;           

             // !!!!!!!!????????????!!!!!!!!!!!!
             // do not generate delta rays with very low energy 
             // if the cut is not small !
             if(T0 < 0.01*Tc) T0=0.01*Tc ;

             // absolute lower limit for T0 
             if(T0<MinDeltaEnergy) T0=MinDeltaEnergy ;

             static const G4double c1N=2.86e-23*MeV/(mm*mm) ;
             static const G4double c2N=c1N*MeV/10. ;

             // compute nb of delta rays to be generated
             G4int N=int(fragment*(c1N*(1.-T0/Tc)+c2N/E)*
                     (aMaterial->GetTotNbOfElectPerVolume())/T0+0.5) ;
             if(N > 0)
             {

      if(print)
      { 
       G4cout << endl;
       G4cout << " subcutoff delta rays-----------START---------------------"
              << "-----------------------------------------" << endl;
       G4cout << "material=" << aMaterial->GetName() << endl; 
       G4cout.precision(5) ; 
       G4cout << "PRE  x,y,z:" <<
                 setw(12) << stepData.GetPreStepPoint()->GetPosition().x() <<
                 setw(12) << stepData.GetPreStepPoint()->GetPosition().y() <<
                 setw(12) << stepData.GetPreStepPoint()->GetPosition().z() <<
                 " safety=" << setw(12) << presafety << endl;  
       G4cout << "PRE    kin.energy=" << setw(12) << E/keV << " keV" <<
                 " dir. x,y,z: " <<
                 setw(12) <<
                 stepData.GetPreStepPoint()->GetMomentumDirection().x() <<
                 setw(12) <<
                 stepData.GetPreStepPoint()->GetMomentumDirection().y() <<
                 setw(12) <<
                 stepData.GetPreStepPoint()->GetMomentumDirection().z() <<
                 endl;
       G4cout << "POST x,y,z:" <<
                 setw(12) << stepData.GetPostStepPoint()->GetPosition().x() <<
                 setw(12) << stepData.GetPostStepPoint()->GetPosition().y() <<
                 setw(12) << stepData.GetPostStepPoint()->GetPosition().z() <<
                 " safety=" << setw(12) << postsafety << endl;  
       G4cout << "POST   kin.energy=" << setw(12) << E/keV << " keV" <<
                 " dir. x,y,z: " <<
                 setw(12) <<
                 stepData.GetPostStepPoint()->GetMomentumDirection().x() <<
                 setw(12) <<
                 stepData.GetPostStepPoint()->GetMomentumDirection().y() <<
                 setw(12) <<
                 stepData.GetPostStepPoint()->GetMomentumDirection().z() <<
                 endl;
       G4cout << " Step=" << setw(12) << "  MeanLoss here=" << MeanLoss/keV
              << " keV" << endl;
       G4cout << setw(6) << N << " delta will be generated with energy between"
              << setw(12) << T0/keV << " keV  and" << setw(12) << Tc/keV <<
                 " keV" << endl; 
      }

               G4double Tkin,Etot,P,T,p,costheta,sintheta,phi,dirx,diry,dirz,
                        Pnew,Px,Py,Pz,delToverTc,
                        TkinStart,MeanLossStart,sumT,delTkin,delLoss,rate,
                        urandom ;
               G4ThreeVector ParticleDirection ;
               G4StepPoint *point ;
 
               TkinStart=E;
               MeanLossStart=MeanLoss;
               sumT=0.;

               Tkin = E ;
               Etot = Tkin+electron_mass_c2 ;
               P    = sqrt(Tkin*(Etot+electron_mass_c2)) ;

               aParticleChange.SetNumberOfSecondaries(N);
               G4int subdelta = 0;
               do {
                    subdelta += 1 ;

                    if((charge<0.)&&(Tc>0.5*Tkin)) Tc=0.5*Tkin ;
                    if((charge>0.)&&(Tc>    Tkin)) Tc=    Tkin ;

                    //check if there is enough energy ....
                    if((Tc > T0)&&(MeanLoss>0.))
                    {
                      delToverTc=1.-T0/Tc ;
                      T=T0/(1.-delToverTc*G4UniformRand()) ;
                      if(T > MeanLoss) T=MeanLoss ;
                      MeanLoss -= T ;
                      p=sqrt(T*(T+2.*electron_mass_c2)) ;

                      costheta = T*(Etot+electron_mass_c2)/(P*p) ;
                      if(costheta<-1.) costheta=-1.;
                      if(costheta> 1.) costheta= 1.;

                      phi=twopi*G4UniformRand() ;
                      sintheta=sqrt(1.-costheta*costheta);
                      dirx=sintheta*cos(phi);
                      diry=sintheta*sin(phi);
                      dirz=costheta;
                    }
                    else
                    {
                      T=0.;
                      p=0.;
                      dirx=0.;
                      diry=0.;
                      dirz=1.;
                    }  

                    sumT += T ;

                    urandom = G4UniformRand() ;
                    // distribute x,y,z along Pre-Post !
                    G4double xd,yd,zd ;
                    xd=x1+frperstep*dx*urandom ;
                    yd=y1+frperstep*dy*urandom ;
                    zd=z1+frperstep*dz*urandom ;
                    G4ThreeVector DeltaPosition(xd,yd,zd) ;
                    DeltaTime=time0+frperstep*dTime*urandom ;
              //    ????????? this or Pre direction or else ?
                    ParticleDirection=stepData.GetPostStepPoint()->
                                      GetMomentumDirection() ;    
                    
                    G4ThreeVector DeltaDirection(dirx,diry,dirz) ;
                    DeltaDirection.rotateUz(ParticleDirection);

                    G4DynamicParticle* theDelta = new G4DynamicParticle ;
                    theDelta->SetDefinition(G4Electron::Electron());
                    theDelta->SetKineticEnergy(T);

                    theDelta->SetMomentumDirection(DeltaDirection.x(),
                                   DeltaDirection.y(),DeltaDirection.z());
                    
      if(print)
      {
        G4cout << endl;
        G4cout << " delta index=" << subdelta ;
        G4cout << "  kin.energy=" << setw(12) << T/keV << " keV" << endl;
        G4cout << "  direction: " 
               << setw(12) << DeltaDirection.x() 
               << setw(12) << DeltaDirection.y() 
               << setw(12) << DeltaDirection.z() << endl; 
        G4cout << "coordinates: " << setw(12) << xd << setw(12) << yd <<
                  setw(12) << zd << endl ;  
        G4cout << "  time=" << setw(12) << DeltaTime << endl;
      }
        
                    // update initial particle,fill ParticleChange
                    Tkin -= T ;
                    Etot  = Tkin+electron_mass_c2 ;
                    Pnew  =sqrt(Tkin*(Etot+electron_mass_c2)) ;
                    Px =(P*ParticleDirection.x()-p*DeltaDirection.x())/Pnew ;
                    Py =(P*ParticleDirection.y()-p*DeltaDirection.y())/Pnew ;
                    Pz =(P*ParticleDirection.z()-p*DeltaDirection.z())/Pnew ;
                    P  = Pnew ;
                    G4ThreeVector ParticleDirectionnew(Px,Py,Pz) ;
                    ParticleDirection = ParticleDirectionnew;

                    G4Track* deltaTrack =
                             new G4Track(theDelta,DeltaTime,DeltaPosition);
                    deltaTrack->
                     SetTouchable(stepData.GetPostStepPoint()->GetTouchable()) ;    
                    deltaTrack->SetParentID(trackData.GetTrackID()) ;

                    aParticleChange.AddSecondary(deltaTrack) ;

                  } while (subdelta<N) ;

                  // update the particle direction and kinetic energy
                  aParticleChange.SetMomentumChange(Px,Py,Pz) ;
                  E = Tkin ;

     if(print)
     {  
       G4cout << endl;
       G4cout << "END    kin.energy=" << setw(12) << E/keV << " keV" <<
                 " dir. x,y,z: " <<
                 setw(12) << Px << setw(12) << Py << setw(12) << Pz << endl;
       G4cout << "END   MeanLoss =" << MeanLoss/keV
              << " keV" << endl;
       delTkin=TkinStart-Tkin;
       delLoss=MeanLossStart-MeanLoss; 
       rate=sumT/MeanLossStart ;
       G4cout << " primary kin.energies (start/end in keV):" << setw(12) <<
                 TkinStart/keV << setw(12) << Tkin/keV << "  difference=" <<
                 delTkin/keV << endl;
       G4cout << "    MeanLoss          (start/end in keV):" << setw(12) <<
                 MeanLossStart/keV << setw(12) << MeanLoss/keV <<
                 "  difference=" << delLoss/keV << endl;
       G4cout << " sum of delta kin. energies=" << setw(12) <<sumT/keV <<
                 " keV    sumTdelta/MeanLossStart=" << setw(12) <<
                 rate << endl;
       G4cout << " subcutoff delta rays-----------END-----------------------"
              << "-----------------------------------------" << endl;
       G4cout << endl;
     } 

               }  
           }
         }
       }
       // !!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!!

       if (MeanLoss < 0.) { MeanLoss = 0.; finalT = E;}
       
       //now the loss with fluctuation
       if ((EnlossFlucFlag) && (MeanLoss > 0.) && (MeanLoss < E))
         {
           finalT = E-GetLossWithFluct(aParticle,aMaterial,MeanLoss);
           if (finalT < 0.) finalT = E-MeanLoss;
         }
     }

  // kill the particle if the kinetic energy <= 0  
  if (finalT <= 0. )
    {
      finalT = 0.;
      if (charge < 0.) aParticleChange.SetStatusChange(fStopAndKill);
      else             aParticleChange.SetStatusChange(fStopButAlive); 
    } 

  // aParticleChange.SetNumberOfSecondaries(0);
  aParticleChange.SetEnergyChange(finalT);
  aParticleChange.SetLocalEnergyDeposit(E-finalT);

  return &aParticleChange;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....

G4double G4eEnergyLossPlus::GetLossWithFluct(const G4DynamicParticle* aParticle,
                                               G4Material* aMaterial,
                                               G4double    MeanLoss)
//  calculate actual loss from the mean loss
//  The model used to get the fluctuation is the same as in Glandz in Geant3.
{
  // check if the material has changed ( cache mechanism)

  if (aMaterial != lastMaterial)
    {
      lastMaterial = aMaterial;
      imat         = aMaterial->GetIndex(); 
      f1Fluct      = aMaterial->GetIonisation()->GetF1fluct();
      f2Fluct      = aMaterial->GetIonisation()->GetF2fluct();
      e1Fluct      = aMaterial->GetIonisation()->GetEnergy1fluct();
      e2Fluct      = aMaterial->GetIonisation()->GetEnergy2fluct();
      e1LogFluct   = aMaterial->GetIonisation()->GetLogEnergy1fluct();
      e2LogFluct   = aMaterial->GetIonisation()->GetLogEnergy2fluct();
      rateFluct    = aMaterial->GetIonisation()->GetRateionexcfluct();
      ipotFluct    = aMaterial->GetIonisation()->GetMeanExcitationEnergy();
      ipotLogFluct = aMaterial->GetIonisation()->GetLogMeanExcEnergy();
    }

  G4double threshold,w1,w2,w3,lnw3,C,prob,
           beta2,suma,e0,Em,loss,lossc ,w;
  G4double a1,a2,a3;
  long p1,p2,p3;
  G4int nb;
  G4double Corrfac, na,alfa,rfac,namean,sa,alfa1,ea,sea;
  G4double dp1,dnmaxDirectFluct,dp3,dnmaxCont2;

  // get particle data
  G4double Tkin   = aParticle->GetKineticEnergy();
  G4double charge = aParticle->GetDefinition()->GetPDGCharge();
  if (charge<0.) threshold =((*G4Electron::Electron()).GetCutsInEnergy())[imat];
  else           threshold =((*G4Positron::Positron()).GetCutsInEnergy())[imat];

  G4double rmass = electron_mass_c2/ParticleMass;
  G4double tau   = Tkin/ParticleMass, tau1 = tau+1., tau2 = tau*(tau+2.);
  G4double Tm    = 2.*electron_mass_c2*tau2/(1.+2.*tau1*rmass+rmass*rmass)
                  -ipotFluct;
  if (Tm < 0.) Tm = 0.;
  else if (Tm > threshold) Tm = threshold;

  w1 = Tm+ipotFluct;
  w2 = w1/ipotFluct;
  w3 = 2.*electron_mass_c2*tau2;
  lnw3 = log(w3);
  beta2 = tau2/(tau1*tau1);

  C = (1.-rateFluct)*MeanLoss/(lnw3-ipotLogFluct-beta2);

  a1 = C*f1Fluct*(lnw3-e1LogFluct-beta2)/e1Fluct;
  a2 = C*f2Fluct*(lnw3-e2LogFluct-beta2)/e2Fluct;
  if (Tm > 0.) a3 = rateFluct*MeanLoss*Tm/(ipotFluct*w1*log(w2));
  else { a1 /= rateFluct; a2 /= rateFluct; a3 = 0.;}
  suma = a1+a2+a3;
  
  //no fluctuation if the loss is too big
  if (suma > MaxExcitationNumber)  return  MeanLoss;

  suma<50.? prob = exp(-suma) : prob = 0.;

  if (prob > probLimFluct)         // very small Step
    {
      e0 = aMaterial->GetIonisation()->GetEnergy0fluct();
      if (Tm <= 0.)
        {
          a1 = MeanLoss/e0;
          p1 = RandPoisson::shoot(a1);
          loss = p1*e0 ;
        }
     else
        {
          Em = Tm+e0;
          a1 = MeanLoss*(Em-e0)/(Em*e0*log(Em/e0));
          p1 = RandPoisson::shoot(a1);
          w  = (Em-e0)/Em;
          // just to save time 
          if (p1 > nmaxDirectFluct)
            {
              dp1 = p1;
              dnmaxDirectFluct=nmaxDirectFluct;
              Corrfac = dp1/dnmaxDirectFluct;
              p1 = nmaxDirectFluct;
            }
          else Corrfac = 1.;

          loss = 0.;
          for (long i=0; i<p1; i++) loss += 1./(1.-w*G4UniformRand());
          loss *= (e0*Corrfac);

        }
    }
    
  else                              // not so small Step
    {
      p1 = RandPoisson::shoot(a1);
      p2 = RandPoisson::shoot(a2);
      loss = p1*e1Fluct+p2*e2Fluct;
      if (loss>0.) loss += (1.-2.*G4UniformRand())*e1Fluct;   
      p3 = RandPoisson::shoot(a3);

      lossc = 0.; na = 0.; alfa = 1.;
      if (p3 > nmaxCont2)
        {
          dp3        = p3;
          dnmaxCont2 = nmaxCont2;
          rfac       = dp3/(dnmaxCont2+dp3);
          namean     = p3*rfac;
          sa         = nmaxCont1*rfac;
          na         = RandGauss::shoot(namean,sa);
          if (na > 0.)
            {
              alfa   = w2*(nmaxCont2+p3)/(w2*nmaxCont2+p3);
              alfa1  = alfa*log(alfa)/(alfa-1.);
              ea     = na*ipotFluct*alfa1;
              sea    = ipotFluct*sqrt(na*(alfa-alfa1*alfa1));
              lossc += RandGauss::shoot(ea,sea);
            }
        }

      nb = G4int(p3-na);
      if (nb > 0)
        {
          w2 = alfa*ipotFluct;
          w  = (w1-w2)/w1;      
          for (G4int k=0; k<nb; k++) lossc += w2/(1.-w*G4UniformRand());
        }
         
      loss += lossc;  
    } 

  return loss ;
}

//....oooOO0OOooo........oooOO0OOooo........oooOO0OOooo........oooOO0OOooo....
   