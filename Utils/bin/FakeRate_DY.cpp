// C++ includes
#include <iostream>
#include <cmath>
#include <vector>
#include <dirent.h>
#include <time.h>
#include <iomanip>

// ROOT includes
#include "TFile.h"
#include "TTree.h"
#include "TString.h"
#include "TClonesArray.h"
#include "TH2F.h"
#include "TCanvas.h"
#include "TMath.h"
#include "TChain.h"
#include "TLorentzVector.h"

// Bacon includes
#include "BaconAna/DataFormats/interface/TElectron.hh"
#include "BaconAna/DataFormats/interface/TMuon.hh"
#include "BaconAna/DataFormats/interface/TJet.hh"
#include "BaconAna/DataFormats/interface/TGenParticle.hh"

using namespace std;
using namespace baconhep;

bool passElectronID(TElectron* elec, const float (&cuts)[10]);
bool passTightMuonID(TMuon* muon);
bool passLooseMuonID(TMuon* muon);

// Loadbar: x = iEvent,n = totEvents, w = width
static inline void loadbar(unsigned int x, unsigned int n, unsigned int r = 20, unsigned int w = 20)
{
    if ( x % (n/r+1) != 0 ) return;
 
    float ratio  =  x/(float)n;
    unsigned int   c      =  ratio * w;
 
    cout << setw(3) << (int)(ratio*100) << "% [";
    for (unsigned int x=0; x<c; x++) cout << "=";
    for (unsigned int x=c; x<w; x++) cout << " ";
    cout << "]\r" << flush;
}

int main(int argc, char* argv[]) {    
    // ----- Variables to be set --------------
    // Selection
    bool eLepton = true; // eLepton = true: Z -> ee
    bool eJet = true; // eJet = true: jet -> e
    bool tightSel = true; // tight or loose lepton selection
    float leptonPt = 20;
    
    // Binning
//     const int nEtaBins = 10, nPtBins = 16;
//     const float etaBins[nEtaBins+1] = {0,0.25,0.5,0.75,1,1.25,1.5,1.75,2,2.25,2.5};
//     const float ptBins[nPtBins+1] = {0,10,20,30,40,50,60,70,80,90,100,120,140,160,180,200,250};
    const int nEtaBinsEl = 6, nPtBinsEl = 10;
    const Double_t etaBinsEl[nEtaBinsEl+1] = {0,0.5,1,1.5,1.75,2,2.5};
    const Double_t ptBinsEl[nPtBinsEl+1] = {20,30,40,50,60,80,100,130,170,220,300};
    const int nEtaBinsMu = 4, nPtBinsMu = 3;
    const Double_t etaBinsMu[nEtaBinsMu+1] = {0,0.75,1.5,2,2.5};
    const Double_t ptBinsMu[nPtBinsMu+1] = {20,30,50,120};
    const Double_t ptBinsMujet[nPtBinsMu+1] = {20,40,60,160};
    
    // ID cuts
    // tightElCuts[barrel/endcap][IDcuts], CSA14 selection, conditions: 50ns, poor detector alignment
//     const float tightElCuts[2][10] = {{0.012,0.024,0.01,0.074,0.0091,0.017,0.026,0.10,1e-6,1},{0.019,0.043,0.029,0.08,0.037,0.065,0.076,0.14,1e-6,1}};
    const float looseElCuts[2][10] = {{0.0181,0.0936,0.0123,0.141,0.0166,0.54342,0.1353,0.24,1e-6,1},{0.0124,0.0642,0.035,0.1115,0.098,0.9187,0.1443,0.3529,1e-6,1}};    
    // veto cuts
    const float tightElCuts[2][10] = {{0.021,0.25,0.012,0.24,0.031,0.5,0.32,0.45,1e-6,2},{0.028,0.23,0.035,0.19,0.22,0.91,0.13,0.45,1e-6,3}}; 
    // CSA14 selection, conditions: 25ns, better detector alignment
//     const float tightElCuts[2][10] = {{0.0091,0.031,0.0106,0.0532,0.0126,0.0116,0.0609,0.1649,1e-6,1},{0.0106,0.0359,0.0305,0.0835,0.0163,0.5999,0.1126,0.2075,1e-6,1}};
//     const float looseElCuts[2][10] = {{0.0181,0.0936,0.0123,0.141,0.0166,0.54342,0.1353,0.24,1e-6,1},{0.0124,0.0642,0.035,0.1115,0.098,0.9187,0.1443,0.3529,1e-6,1}};
    
    // Directories
    int maxInFiles=250;
    TString outDirPNG = "/afs/cern.ch/user/j/jlauwers/www/protected/VBS/TP/FakeRate_rebin/";
    TString outDirROOT = "/afs/cern.ch/work/j/jlauwers/VBS/TP/FakeRate/Results/";
    TString inDir = "/afs/cern.ch/work/j/jlauwers/VBS/TP/FakeRate/eos/cms/store/group/upgrade/delphes/VBS_FakeRate/";
//     TString inDir = "/afs/cern.ch/work/j/jlauwers/VBS/TP/FakeRate/BaconTrees/";
    
    // Constants
    const Float_t pi = 3.1416;
    
    // Verbose output
    int verbose = 1; // 0: no messages - 1: basic output and load bar - 2: calculate lepton efficiency - 3: all debug information 
    // ----------------------------------------
    
    // Parse command line parameters
    if (argc < 2) {
        cout << cout << "Studying Z -> " << (eLepton?"ee":"mumu") << ", jet -> " << (eJet?"e":"mu") << " process" << endl;
    }
    else if( string(argv[1]) == "ee" ){
        eLepton = true;
        eJet = true;
        if( verbose > 0 ) cout << "Studying W -> e, jet -> e process" << endl;
    }
    else if( string(argv[1]) == "em" ){
        eLepton = true;
        eJet = false;
        if( verbose > 0 ) cout << "Studying W -> e, jet -> mu process" << endl;
    }
    else if( string(argv[1]) == "me" ){
        eLepton = false;
        eJet = true;
        if( verbose > 0 ) cout << "Studying W -> mu, jet -> e process" << endl;
    }
    else if( string(argv[1]) == "mm" ){
        eLepton = false;
        eJet = false;
        if( verbose > 0 ) cout << "Studying W -> mu, jet -> mu process" << endl;
    }
    
    // use correct inDir
    if( eLepton ) inDir += "DYToEE_Shashlik/";
    else inDir += "DYToMM_Shashlik/";
        
    // Set timer
    clock_t t1,t2;
    t1=clock();
    
    // Add all BaconTrees to a chain
    TChain* tree = new TChain("Events");
    DIR *dpdf;
    struct dirent *epdf;
    int nFiles = 0;
    if( verbose > 2 ) maxInFiles=1;

    dpdf = opendir(inDir);
    if (dpdf != NULL){
        while ((epdf = readdir(dpdf))){
            string fname = epdf->d_name;
            if (fname != "." && fname != "..") {
                tree->Add(inDir+fname);
                nFiles++;
                
                if( verbose > 2 ) cout << "Adding file: " << epdf->d_name << endl;
                if( nFiles == maxInFiles ) break;
            }
        }
    }
    if( verbose > 0 ) cout << "Added " << nFiles << " files to chain." << endl;
    
    tree->SetBranchStatus("*",0);
    tree->SetBranchStatus("Electron*",1);
    tree->SetBranchStatus("Muon*",1);
    tree->SetBranchStatus("Jet04*",1);
    tree->SetBranchStatus("GenParticle*",1);
    
    TClonesArray *fElectron = new TClonesArray("baconhep::TElectron");
    TClonesArray *fMuon = new TClonesArray("baconhep::TMuon");
    TClonesArray *fJet = new TClonesArray("baconhep::TJet");
    TClonesArray *fGenParticle = new TClonesArray("baconhep::TGenParticle");
    tree->SetBranchAddress("Electron", &fElectron);
    tree->SetBranchAddress("Muon", &fMuon);
    tree->SetBranchAddress("Jet04", &fJet);
    tree->SetBranchAddress("GenParticle", &fGenParticle);
    
    TString strSel = "Z_to";
    if( eLepton ) strSel += "_ee";
    else strSel += "_mumu";
    if( eJet ) strSel += "_jet_to_e";
    else strSel += "_jet_to_mu";
    
    TH1::SetDefaultSumw2();
    TH2F *hJetNum, *hLeptNum, *hDenom, *hDenomJet=0;
    TH2D *hBinCentres;
    if( eJet ) {
        hJetNum= new TH2F("Jet_Numerator_"+strSel,"Jet_Numerator_"+strSel,nEtaBinsEl, etaBinsEl, nPtBinsEl, ptBinsEl);
        hLeptNum = new TH2F("Lepton_Numerator_"+strSel,"Lepton_Numerator_"+strSel,nEtaBinsEl, etaBinsEl, nPtBinsEl, ptBinsEl);
        hDenom = new TH2F("Denominator_"+strSel,"Denominator_"+strSel,nEtaBinsEl, etaBinsEl, nPtBinsEl, ptBinsEl);
        hBinCentres = new TH2D("Pt_centre_"+strSel,"Pt_centre_"+strSel,nEtaBinsEl, etaBinsEl, nPtBinsEl, ptBinsEl);
    }
    else {
        hJetNum= new TH2F("Jet_Numerator_"+strSel,"Jet_Numerator_"+strSel,nEtaBinsMu, etaBinsMu, nPtBinsMu, ptBinsMujet);
        hLeptNum = new TH2F("Lepton_Numerator_"+strSel,"Lepton_Numerator_"+strSel,nEtaBinsMu, etaBinsMu, nPtBinsMu, ptBinsMu);
        hDenom = new TH2F("Denominator_"+strSel,"Denominator_"+strSel,nEtaBinsMu, etaBinsMu, nPtBinsMu, ptBinsMu);
        hDenomJet = new TH2F("Jet_Denominator_"+strSel,"Jet_Denominator_"+strSel,nEtaBinsMu, etaBinsMu, nPtBinsMu, ptBinsMujet);
        hBinCentres = new TH2D("Pt_centre_"+strSel,"Pt_centre_"+strSel,nEtaBinsMu, etaBinsMu, nPtBinsMu, ptBinsMujet);
    }
    TH2F *hPtMigrationCentral=0, *hPtMigrationForward=0, *hEMigrationCentral=0, *hEMigrationForward=0;
    TH1D *hMigrationBinCentres;
    if( eJet ) {
        hPtMigrationCentral = new TH2F("Pt_migration_barrel_"+strSel,"Pt_migration_barrel_"+strSel,nPtBinsEl, ptBinsEl, 60, -50., 250.);
        hPtMigrationForward = new TH2F("Pt_migration_endcap_"+strSel,"Pt_migration_endcap_"+strSel,nPtBinsEl, ptBinsEl, 60, -50., 250.);
        hEMigrationCentral = new TH2F("E_migration_barrel_"+strSel,"E_migration_barrel_"+strSel,nPtBinsEl, ptBinsEl, 60, -50., 250.);
        hEMigrationForward = new TH2F("E_migration_endcap_"+strSel,"E_migration_endcap_"+strSel,nPtBinsEl, ptBinsEl, 60, -50., 250.);
        hMigrationBinCentres = new TH1D("Pt_migration_centre_"+strSel,"Pt_migration_centre_"+strSel, nPtBinsEl, ptBinsEl);
    }
    else {
        hPtMigrationCentral = new TH2F("Pt_migration_barrel_"+strSel,"Pt_migration_"+strSel,nPtBinsMu, ptBinsMujet, 60, -50., 250.);
        hPtMigrationForward = new TH2F("Pt_migration_endcap_"+strSel,"Pt_migration_endcap_"+strSel,nPtBinsMu, ptBinsMujet, 60, -50., 250.);
        hEMigrationCentral = new TH2F("E_migration_barrel_"+strSel,"E_migration_barrel_"+strSel,nPtBinsMu, ptBinsMujet, 60, -50., 250.);
        hEMigrationForward = new TH2F("E_migration_endcap_"+strSel,"E_migration_endcap_"+strSel,nPtBinsMu, ptBinsMujet, 60, -50., 250.);
        hMigrationBinCentres = new TH1D("Pt_migration_centre_"+strSel,"Pt_migration_centre_"+strSel, nPtBinsMu, ptBinsMujet);
    }
    TH2F *hZpt = new TH2F("Z_pt_"+strSel,"Z_pt_"+strSel,15,0,300,18,30,300);
    
    int lepElec=0, lepMuon=0, jetElec=0, jetMuon=0, l1NotMatched=0, l2NotMatched=0;
    float elecEffDenom=0, elecEffNum=0, muonEffDenom=0, muonEffNum=0;
    int jetlep2GenMatch=0, wLept=0;
    
    // Loop over events
    int nevents = tree->GetEntries(); // GetEntriesFast fails for chain
    if( verbose > 0 ) cout << "nevents: " << nevents << endl;
    for( int i = 0; i < nevents; ++i ) {
        if( verbose > 0 ) loadbar(i+1,nevents);
        tree->GetEntry(i);
        int nElectrons = fElectron->GetEntriesFast();
        int nMuons = fMuon->GetEntriesFast();
        int nGenParticles = fGenParticle->GetEntriesFast();
        if( verbose > 2 ) cout << "nElectrons: " << nElectrons << ", nMuons: " << nMuons << endl;
        
        // Calculate lepton efficiencies
        if( verbose > 0 ) {
            for( int iE = 0; iE < nElectrons; ++iE ) {
                TElectron *elec = (TElectron*)((*fElectron)[iE]);
                if( fabs(elec->eta) < 2.5 && elec->pt > leptonPt) {
                    elecEffDenom++;
                    bool inEndcap = fabs(elec->scEta) > 1.479;
                    bool passSel;
                    if( tightSel) passSel = passElectronID(elec, tightElCuts[inEndcap] );
                    else passSel = passElectronID(elec, looseElCuts[inEndcap] );
                    if( passSel ) elecEffNum++;
                }
            }
            for( int iM = 0; iM < nMuons; ++iM ) {
                TMuon *muon = (TMuon*)((*fMuon)[iM]);
                if( fabs(muon->eta) < 2.5 && muon->pt > leptonPt) {
                    muonEffDenom++;
                    bool passSel;
                    if( tightSel) passSel = passTightMuonID(muon);
                    else passSel =  passLooseMuonID(muon);
                    if( passSel ) muonEffNum++;
                }
            }
        }
        
        // -- Select at least two lepton with pt > leptonPt GeV that matches a genLepton --
        vector<int> genLeptIndex; // extra gen leptons: eta > 2.5
        vector<float> leptonEta, leptonPhi; // Has to be removed from jet collection
        float Zpt=0;
        unsigned int nLeptonsReq=0;
        bool skipEvent = true; // check if there is a Z boson
        
        // Z -> e e
        if( eLepton ) {
            // Count number of electrons expected in tracker
            for( int iG = 0; iG < nGenParticles; ++iG ) {
                TGenParticle *genP = (TGenParticle*)((*fGenParticle)[iG]); 
                if( abs(genP->pdgId) == 23 ) skipEvent = false;
                if( abs(genP->pdgId) == 11 && genP->status == 1 && genP->parent!=-2 && fabs(genP->eta) < 2.8 ) { // 2.8 to correct for dR
                    bool foundZ = false, stuck = false, inTracker = fabs(genP->eta) < 2.5;
                    do {
                        if(genP->parent!=-2) {
                            TGenParticle *genTemp = (TGenParticle*)((*fGenParticle)[genP->parent]);
                            genP = genTemp;
                        }
                        else stuck = true;
                        if( genP->pdgId == 23) {
                            foundZ = true;
                        }
                    } while( !stuck ); 
                    if( foundZ ) {
                        genLeptIndex.push_back(iG);
                        if( inTracker ) nLeptonsReq++;
                        Zpt = genP->pt;
                    }
                }
            }
            if( skipEvent ) continue;
            if( verbose > 2 ) cout << "Required electrons in tracker: " << nLeptonsReq << endl;
        
//             if( nElectrons < 2 ) continue;
            for( int iE = 0; iE < nElectrons; ++iE ) {
                TElectron *elec = (TElectron*)((*fElectron)[iE]);
//                 bool inEndcap = fabs(elec->scEta) > 1.479;
                bool passSel=fabs(elec->eta) < 2.5;
//                 if( tightSel) passSel = elec->pt > leptonPt && passElectronID(elec, tightElCuts[inEndcap] );
//                 else passSel = elec->pt > leptonPt && passElectronID(elec, looseElCuts[inEndcap] );
                if( passSel ) {
                    l1NotMatched++;
                    if( verbose > 2 ) cout << "Electron " << iE <<" passed lepton id" << endl;
                    
                    // Match with genParticle
                    for( vector<int>::iterator iG = genLeptIndex.begin(); iG != genLeptIndex.end(); ++iG ) {
                        TGenParticle *genP = (TGenParticle*)((*fGenParticle)[*iG]); 
                        Double_t dR = TMath::Sqrt( TMath::Power(genP->eta - elec->eta, 2) + TMath::Power(fabs(fabs(genP->phi - elec->phi)-pi)-pi, 2) );
                        bool sameCharge = genP->pdgId == (elec->q)*-11.;
                        if( verbose > 2 ) cout << "dR: " << (dR < 0.3) << ", sameCharge: " << sameCharge << endl;
                        
                        if( sameCharge && dR < 0.3 ) {
                            lepElec++;
                            if( verbose > 2 ) cout << "Matched with genElectron " << *iG << " from Z" << endl;
                            leptonEta.push_back(elec->eta);
                            leptonPhi.push_back(elec->phi);
                            if( fabs(genP->eta) >= 2.5 ) nLeptonsReq++;  // corection for genLepton outside tracker en reco lepton inside
                            genLeptIndex.erase(iG); // genLepton can only be matched to one reco lepton
                            wLept++;
                            l1NotMatched--;
                            break;                                            
                        }
                    }
                }
            }
            if( leptonEta.size() != nLeptonsReq ) continue;
        }
        
        // Z -> mu mu
        else {
            // Count number of muons expected in tracker
            for( int iG = 0; iG < nGenParticles; ++iG ) {
                TGenParticle *genP = (TGenParticle*)((*fGenParticle)[iG]); 
                if( abs(genP->pdgId) == 23 ) skipEvent = false;
                if( abs(genP->pdgId) == 13 && genP->status == 1 && genP->parent!=-2 && fabs(genP->eta) < 2.8 ) {
                    bool foundZ = false, stuck = false, inTracker = fabs(genP->eta) < 2.5;
                    do {
                        if(genP->parent!=-2) {
                            TGenParticle *genTemp = (TGenParticle*)((*fGenParticle)[genP->parent]);
                            genP = genTemp;
                        }
                        else stuck = true;
                        if( genP->pdgId == 23) {
                            foundZ = true;
                        }
                    } while( !stuck ); 
                    if( foundZ ) {
                        genLeptIndex.push_back(iG);
                        if( inTracker ) nLeptonsReq++;
                        Zpt = genP->pt;
                    }
                }
            }
            if( skipEvent ) continue;
            if( verbose > 2 ) cout << "Required muons in tracker: " << nLeptonsReq << endl;
            
//             if( nMuons < 2 ) continue;
            for( int iM = 0; iM < nMuons; ++iM ) {
                TMuon *muon = (TMuon*)((*fMuon)[iM]);
                bool passSel= fabs(muon->eta) < 2.5 && ((muon->typeBits)/32)%2; // otherwise too many muons
//                 if( tightSel) passSel = muon->pt > leptonPt && passTightMuonID(muon);
//                 /*else*/ passSel =  muon->pt > leptonPt /*&& passLooseMuonID(muon)*/;
                if( passSel ) {
                    l1NotMatched++;
                    if( verbose > 2 ) cout << "Muon " << iM << " passed lepton id" << endl;
                    
                    // Match with genParticle
                    for( vector<int>::iterator iG = genLeptIndex.begin(); iG != genLeptIndex.end(); ++iG ) {
                        if( verbose > 2 ) cout << "Genleptons left:  " << genLeptIndex.size() << endl;
                        TGenParticle *genP = (TGenParticle*)((*fGenParticle)[*iG]); 
                        Double_t dR = TMath::Sqrt( TMath::Power(genP->eta - muon->eta, 2) + TMath::Power(fabs(fabs(genP->phi - muon->phi)-pi)-pi, 2) );
                        bool sameCharge = genP->pdgId == (muon->q)*-13.;
                        if( verbose > 2 ) cout << "dR: " << (dR < 0.3) << ", sameCharge: " << sameCharge << "gen charge: " << genP->pdgId << ", reco charge: " << (muon->q)*-13 << endl;
                        
                         if( sameCharge && dR < 0.3 ) {
                            lepMuon++;
                            if( verbose > 2 ) cout << "Matched with genMuon " << *iG << " from Z" << endl;
                            leptonEta.push_back(muon->eta);
                            leptonPhi.push_back(muon->phi);
                            if( fabs(genP->eta) >= 2.5 ) nLeptonsReq++; // corection for genLepton outside tracker en reco lepton inside
                            genLeptIndex.erase(iG); // genLepton can only be matched to one reco lepton
                            wLept++;
                            l1NotMatched--;
                            break;                                            
                        }
                    }
                }
            }
            if( leptonEta.size() != nLeptonsReq ) continue;
        }
        
        // -- Check if there is a third lepton --
        int secLeptIndex = -1; // more than 1 leptons from jets is negligible
        TMuon *muon=0; 
        TElectron *elec=0;
        
        // Jet -> e
        if( eJet ) {
            for( int iE = 0; iE < nElectrons; ++iE ) {
                TElectron *elec = (TElectron*)((*fElectron)[iE]);
                bool inEndcap = fabs(elec->scEta) > 1.479;
                bool passSel;
                if( tightSel) passSel = elec->pt > leptonPt && fabs(elec->eta) < 2.5 && passElectronID(elec, tightElCuts[inEndcap] );
                else passSel = elec->pt > leptonPt && fabs(elec->eta) < 2.5 && passElectronID(elec, looseElCuts[inEndcap] );
                if( passSel ) {
                    if( eLepton ) { // skip previously matched electron
                        bool isZLepton = false;
                        for( unsigned int iLep=0; iLep  < leptonEta.size(); ++iLep ) {
                            if( leptonEta[iLep] == elec->eta || leptonPhi[iLep] == elec->phi) isZLepton = true; 
                        }
                        if( isZLepton ) continue;
                        
                        // Match with genParticle to be sure it doesn't come from Z                    
                        for( int iG = 0; iG < nGenParticles; ++iG ) {
                            TGenParticle *genP = (TGenParticle*)((*fGenParticle)[iG]); 
                            Double_t dR = TMath::Sqrt( TMath::Power(genP->eta - elec->eta, 2) + TMath::Power(fabs(fabs(genP->phi - elec->phi)-pi)-pi, 2) );
                            bool sameCharge = genP->pdgId == (elec->q)*-11.;
                            if( abs(genP->pdgId) == 11 && genP->status == 1 && genP->parent!=-2 && fabs(genP->eta) < 2.8 ) {
                                bool foundZ = false, stuck = false;
                                do {
                                    if(genP->parent!=-2) {
                                        TGenParticle *genTemp = (TGenParticle*)((*fGenParticle)[genP->parent]);
                                        genP = genTemp;
                                    }
                                    else stuck = true;
                                    if( genP->pdgId == 23) {
                                        foundZ = true;
                                    }
                                } while( !stuck ); 
                                if( foundZ && dR < 0.3 && sameCharge ) {
                                    isZLepton=true;
                                    break;
                                }
                            }
                        }
                        if( isZLepton ) continue;
                    }
                    secLeptIndex = iE;
                    if( verbose > 2 ) cout << "Electron " << iE <<" passed lepton id" << endl;
                    jetElec++;
                        
                    break;
                }
            } 
            if( secLeptIndex >= 0 ) 
                elec = (TElectron*)((*fElectron)[secLeptIndex]);
        }
        
        // Jet -> mu
        else {
            for( int iM = 0; iM < nMuons; ++iM ) {
                TMuon *muon = (TMuon*)((*fMuon)[iM]);
                bool passSel;
                if( tightSel) passSel = muon->pt > leptonPt && fabs(muon->eta) < 2.5 && passTightMuonID(muon);
                else passSel = muon->pt > leptonPt && fabs(muon->eta) < 2.5 && passLooseMuonID(muon);
                if( passSel ) {
                    if( !eLepton ) { // skip previously matched muon
                        bool isZLepton = false;
                        for( unsigned int iLep=0; iLep  < leptonEta.size(); ++iLep ) {
                            if( leptonEta[iLep] == muon->eta && leptonPhi[iLep] == muon->phi) isZLepton = true; 
                        }
                        if( isZLepton ) continue;
                        
                        // Match with genParticle to be sure it doesn't come from Z                    
                        for( int iG = 0; iG < nGenParticles; ++iG ) {
                            TGenParticle *genP = (TGenParticle*)((*fGenParticle)[iG]); 
                            Double_t dR = TMath::Sqrt( TMath::Power(genP->eta - muon->eta, 2) + TMath::Power(fabs(fabs(genP->phi - muon->phi)-pi)-pi, 2) );
                            bool sameCharge = genP->pdgId == (muon->q)*-13.;
                            if( abs(genP->pdgId) == 13 && genP->status == 1 && genP->parent!=-2 && fabs(genP->eta) < 2.8 ) {
                                bool foundZ = false, stuck = false;
                                do {
                                    if(genP->parent!=-2) {
                                        TGenParticle *genTemp = (TGenParticle*)((*fGenParticle)[genP->parent]);
                                        genP = genTemp;
                                    }
                                    else stuck = true;
                                    if( genP->pdgId == 23) {
                                        foundZ = true;
                                    }
                                } while( !stuck ); 
                                if( foundZ && dR < 0.3 && sameCharge ) {
                                    isZLepton=true;
                                    break;
                                }
                            }
                        }
                        if( isZLepton ) continue;
                    }
                    
                    secLeptIndex = iM;
                    if( verbose > 2 ) cout << "Muon " << iM <<" passed lepton id" << endl;
                    jetMuon++;
                        
                    break;
                }
            } 
            if( secLeptIndex >= 0 ) 
                muon = (TMuon*)((*fMuon)[secLeptIndex]);          
        }
        
        // -- Loop over jets --
        int nJets = fJet->GetEntriesFast();
        for( int iJ = 0; iJ < nJets; ++iJ ) {
            TJet *jet = (TJet*)((*fJet)[iJ]);
            
            // Skip Z leptons 
            bool skipJet = false;
            for( unsigned int iLep=0; iLep  < leptonEta.size(); ++iLep ) {
                 Double_t dR = TMath::Sqrt( TMath::Power(leptonEta[iLep] - jet->eta, 2) + TMath::Power(fabs(fabs(leptonPhi[iLep] - jet->phi)-pi)-pi, 2) );
                 if( dR < 0.3 ) skipJet = true;
            }           
            if( skipJet ) continue;
            
            // check Z pt vs jet pt
            hZpt->Fill(Zpt,jet->pt);
            
            // Fill histograms
            hDenom->Fill(fabs(jet->eta), jet->pt);
            if( !eJet ) hDenomJet->Fill(fabs(jet->eta), jet->pt);
            int binNumber = hBinCentres->FindBin(fabs(jet->eta),jet->pt);
            hBinCentres->SetBinContent(binNumber, hBinCentres->GetBinContent(binNumber) + jet->pt );
            if( secLeptIndex >= 0 ) {
                Double_t dR;
                if(eJet) dR = TMath::Sqrt( TMath::Power(elec->eta - jet->eta, 2) + TMath::Power(fabs(fabs(elec->phi - jet->phi)-pi)-pi, 2) );
                else dR = TMath::Sqrt( TMath::Power(muon->eta - jet->eta, 2) + TMath::Power(fabs(fabs(muon->phi - jet->phi)-pi)-pi, 2) );
                if( dR < 0.3 ) {
                    if(eJet) {
                        hLeptNum->Fill(fabs(elec->eta), elec->pt);
                        TLorentzVector jet4V;
                        jet4V.SetPtEtaPhiM(jet->pt, jet->eta, jet->phi, jet->mass);
                        
                        if( fabs(jet->eta) < 1.5 ) {
                            hPtMigrationCentral->Fill(jet->pt, jet->pt - elec->pt);
                            hEMigrationCentral->Fill(jet4V.E(), jet->pt - elec->pt);
                        }
                        else {
                            hPtMigrationForward->Fill(jet->pt, jet->pt - elec->pt);
                            hEMigrationForward->Fill(jet4V.E(), jet->pt - elec->pt);
                        }
                    }
                    else {
                        hLeptNum->Fill(fabs(muon->eta), muon->pt);
                        TLorentzVector jet4V;
                        jet4V.SetPtEtaPhiM(jet->pt, jet->eta, jet->phi, jet->mass);
                        
                        if( fabs(jet->eta) < 1.5 ) {
                            hPtMigrationCentral->Fill(jet->pt, jet->pt - muon->pt);
                            hEMigrationCentral->Fill(jet4V.E(), jet->pt - muon->pt);
                        }
                        else {
                            hPtMigrationForward->Fill(jet->pt, jet->pt - muon->pt);
                            hEMigrationForward->Fill(jet4V.E(), jet->pt - muon->pt);
                        }
                    }
                    int binNumber = hMigrationBinCentres->FindBin(jet->pt);
                    hMigrationBinCentres->SetBinContent(binNumber, hMigrationBinCentres->GetBinContent(binNumber) + jet->pt );
                        
                    hJetNum->Fill(fabs(jet->eta), jet->pt);
                    secLeptIndex = -1; // match with only 1 jet
                    if( verbose > 2 ) cout << "Matched with jet" << endl;
                }                
            }
        }
        // Lepton should be matched
        if( secLeptIndex >= 0 ) {
            l2NotMatched++;
            if( verbose > 2 && eJet) {
                cout << "Electron not matched to jet, eta: " << elec->eta << ", pt: " << elec->pt << endl;
                    
                for( int iG = 0; iG < nGenParticles; ++iG ) {
                    TGenParticle *genP = (TGenParticle*)((*fGenParticle)[iG]); 
                    if( abs(genP->pdgId == 11) /*((elec->q)*-11.)*/ && genP->status == 1 && genP->parent!=-2 && fabs(genP->eta) < 2.5 ) {
                        Double_t dR = TMath::Sqrt( TMath::Power(genP->eta - elec->eta, 2) + TMath::Power(fabs(fabs(genP->phi - elec->phi)-pi)-pi, 2) );
                        if( dR > 0.3 ) continue;
                        bool foundZ = false, stuck = false;
                        do {
                            if(genP->parent!=-2) {
                                TGenParticle *genTemp = (TGenParticle*)((*fGenParticle)[genP->parent]);
                                genP = genTemp;
                            }
                            else stuck = true;
                            if( genP->pdgId == 23) {
                                foundZ = true;
                            }
                        } while( !stuck ); 
                        if( foundZ ) cout << "Electron matched to Z" << endl;
                    }
                }
            }
        }
    }    
    
    // Output
    cout << "Electron efficiency: " << elecEffNum/elecEffDenom << ", Muon efficiency: " << muonEffNum/muonEffDenom << endl;
    cout << "# elec1 passing sel and matched to gen electron: " << lepElec << ", # muons1 passing sel and matched to gen muon: " << lepMuon << endl;
    cout << "# elec2 passing sel: " << jetElec << ", # muons2 passing sel: " << jetMuon << endl;
    cout << "lep1 not matched with Z: " << l1NotMatched << ", lep2 not matched with jet: " << l2NotMatched << endl;
    cout << "lep1 matched to Z: " << wLept << endl;
    cout << "lept2 matched with a gen lepton: " << jetlep2GenMatch << endl;
    cout << "Total fake rate: " << hJetNum->Integral()/hDenom->Integral() << endl;
    
    // -- Calculate and draw fake rate --
    TFile* outFile = new TFile(outDirROOT+"FakeRate_DY_v6.root","UPDATE");
    hLeptNum->Write();
    hJetNum->Write();
    hDenom->Write();
    if( !eJet )hDenomJet->Write();
    hPtMigrationCentral->Write();
    hEMigrationCentral->Write();
    hPtMigrationForward->Write();
    hEMigrationForward->Write();
    hZpt->Write();
    
    // Bin centres 
    if( eJet ) hBinCentres->Divide(hDenom);
    else hBinCentres->Divide(hDenomJet);
    TH1D *hPtMigBins = hPtMigrationCentral->ProjectionX("_ptbinsce");
    hPtMigBins->Add( hPtMigrationForward->ProjectionX("_ptbinsfw") );
    hMigrationBinCentres->Divide(hPtMigBins);
    hBinCentres->Write();
    hMigrationBinCentres->Write();

    TString strTitle = "Fake_rate_";
    strTitle += strSel;
    
    TH2F *hLeptFakeRate = (TH2F*) hLeptNum->Clone(strTitle+"_leptNum");
    hLeptFakeRate->Divide(hDenom);
    hLeptFakeRate->SetTitle(strTitle+"_leptNum");
    
    TH2F *hJetFakeRate = (TH2F*) hJetNum->Clone(strTitle+"_jetNum");
    hJetFakeRate->Divide(hDenom);
    hJetFakeRate->SetTitle(strTitle+"_jetNum");
    
    TCanvas *c1 = new TCanvas("c1","c1");
    TH1D *hJetFakeRate_eta =  hJetNum->ProjectionX(strTitle+"_jetNum_eta", 0, -1, "e");
    TH1D *hDenom_eta =  hDenom->ProjectionX("Denominator_jetNum_eta", 0, -1, "e");
    hJetFakeRate_eta->Divide(hDenom_eta);
    hJetFakeRate_eta->GetXaxis()->SetTitle("eta");
    hJetFakeRate_eta->Draw();
    c1->Print(outDirPNG+strTitle+"_jetNum_eta.png","png");
    hJetFakeRate_eta->Write();
    
    TCanvas *c2 = new TCanvas("c2","c2");
    TH1D *hJetFakeRate_pt =  hJetNum->ProjectionY(strTitle+"_jetNum_pt", 0, -1, "e");
    TH1D *hDenom_pt =  hDenom->ProjectionY("Denominator_jetNum_pt", 0, -1, "e");
    hJetFakeRate_pt->Divide(hDenom_pt);
    hJetFakeRate_pt->GetXaxis()->SetTitle("pt");
    hJetFakeRate_pt->Draw();
    c2->Print(outDirPNG+strTitle+"_jetNum_pt.png","png");
    hJetFakeRate_pt->Write();
    
    TCanvas *c3 = new TCanvas("c3","c3");
    hLeptFakeRate->GetXaxis()->SetTitle("eta");
    hLeptFakeRate->GetYaxis()->SetTitle("pt");
    hLeptFakeRate->Draw("TEXT");
    c3->Print(outDirPNG+strTitle+"_leptNum"+".png","png");
    hLeptFakeRate->Write();
    
    TCanvas *c4 = new TCanvas("c4","c4");
    hJetFakeRate->GetXaxis()->SetTitle("eta");
    hJetFakeRate->GetYaxis()->SetTitle("pt");
    hJetFakeRate->Draw("TEXT");
    c4->Print(outDirPNG+strTitle+"_jetNum"+".png","png");
    hJetFakeRate->Write();
    outFile->Delete();
    
    t2=clock();
    float diff ((float)t2-(float)t1);
    cout<< " Total runtime: " << diff/CLOCKS_PER_SEC <<endl;
}

bool passElectronID(TElectron* elec, const float (&cuts)[10] ) {  
    return( fabs(elec->dEtaIn) < cuts[0] &&
            fabs(elec->dPhiIn) < cuts[1] &&
            elec->sieie  < cuts[2] &&
            elec->hovere < cuts[3] &&
            fabs(elec->d0) < cuts[4] &&
            fabs(elec->dz) < cuts[5] &&
//             fabs(elec->eoverp)  < cuts[6] && // eoverp = E/p  not what i need -> missing
            ((elec->chHadIso03 + max(elec->gammaIso03+elec->neuHadIso03-0.5* elec->puIso03,0.0))/elec->pt) < cuts[7] && // electron iso
            (!elec->isConv) &&
            elec->nMissingHits <= cuts[9] 
          );    
}

bool passTightMuonID(TMuon* muon) {
    return( ((muon->typeBits)/2)%2 &&
            ((muon->typeBits)/32)%2 &&
            muon->tkNchi2 < 10 && //
            muon->nValidHits > 0 &&
            muon->nMatchStn > 1 &&
            fabs(muon->d0) < 0.2 && // d0 = -dxy
            fabs(muon->dz) < 0.5 &&
            muon->nPixHits > 0 &&
            muon->nTkLayers > 5 &&
           ((muon->chHadIso04 + max(muon->gammaIso04+muon->neuHadIso04-0.5* muon->puIso04,0.0))/muon->pt) < 0.3 // muon iso  
    );  
}

bool passLooseMuonID(TMuon* muon) {
    return( ((muon->typeBits)/32)%2 &&
            ( ((muon->typeBits)/2)%2 || ((muon->typeBits)/4)%2 ) &&
            ((muon->chHadIso04 + max(muon->gammaIso04+muon->neuHadIso04-0.5* muon->puIso04,0.0))/muon->pt) < 0.3 // muon iso  
          );
}
        
        
