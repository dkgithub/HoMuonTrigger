// -*- C++ -*-
// 
/**

 Description: [one line class summary]

 Implementation:
     [Notes on implementation]
 */
//
// Original Author:  Christopher Anelli  
//         Created:  Fri, 16 May 2014 04:20:05 GMT
// $Id$
//
//

//hoMuonAnalyzer header file
#include "../interface/hoMuonAnalyzer.h"

#include <DataFormats/CaloRecHit/interface/CaloRecHit.h>
#include <DataFormats/Candidate/interface/LeafCandidate.h>
#include <DataFormats/Common/interface/HandleBase.h>
#include <DataFormats/Common/interface/Ref.h>
#include <DataFormats/Common/interface/RefToBase.h>
#include <DataFormats/Common/interface/SortedCollection.h>
#include <DataFormats/Common/interface/View.h>
#include <DataFormats/DetId/interface/DetId.h>
#include <DataFormats/GeometryVector/interface/GlobalPoint.h>
#include <DataFormats/GeometryVector/interface/GlobalVector.h>
#include "DataFormats/GeometryVector/interface/GlobalTag.h"
#include <DataFormats/GeometryVector/interface/Phi.h>
#include <DataFormats/GeometryVector/interface/PV3DBase.h>
#include <DataFormats/HcalDetId/interface/HcalDetId.h>
#include <DataFormats/HcalDetId/interface/HcalSubdetector.h>
#include <DataFormats/HcalRecHit/interface/HcalRecHitCollections.h>
#include <DataFormats/HcalRecHit/interface/HORecHit.h>
#include <DataFormats/HepMCCandidate/interface/GenParticle.h>
#include <DataFormats/L1Trigger/interface/L1MuonParticle.h>
#include <DataFormats/Math/interface/deltaR.h>
#include <DataFormats/Provenance/interface/EventID.h>
#include <DataFormats/TrajectorySeed/interface/PropagationDirection.h>
#include <FWCore/Common/interface/EventBase.h>
#include <FWCore/Framework/interface/ESHandle.h>
#include <FWCore/Framework/interface/Event.h>
#include <FWCore/Framework/interface/EventSetup.h>
#include <FWCore/Framework/interface/EventSetupRecord.h>
#include <FWCore/Framework/interface/MakerMacros.h>
#include <FWCore/Framework/src/Factory.h>
#include <FWCore/Framework/src/WorkerMaker.h>
#include <FWCore/ParameterSet/interface/ParameterSet.h>
#include <FWCore/ParameterSet/interface/ParameterSetDescription.h>
#include <FWCore/ParameterSet/interface/ParameterSetDescriptionFiller.h>
#include <FWCore/ParameterSet/interface/ParameterSetDescriptionFillerPluginFactory.h>
#include <FWCore/PluginManager/interface/PluginFactory.h>
#include <Geometry/CaloGeometry/interface/CaloGeometry.h>
#include <Geometry/Records/interface/CaloGeometryRecord.h>
#include <MagneticField/Records/interface/IdealMagneticFieldRecord.h>
#include <Math/GenVector/DisplacementVector3D.h>
#include <RecoMuon/MuonIdentification/interface/MuonHOAcceptance.h>
#include "RecoLocalCalo/HcalRecAlgos/interface/HcalSeverityLevelComputer.h"
#include "RecoLocalCalo/HcalRecAlgos/interface/HcalSeverityLevelComputerRcd.h"
#include "RecoLocalCalo/HcalRecAlgos/interface/HcalSimpleRecAlgo.h"

#include <SimDataFormats/CaloHit/interface/PCaloHit.h>
#include <SimDataFormats/GeneratorProducts/interface/GenEventInfoProduct.h>
#include <TrackingTools/TrackAssociator/interface/TrackAssociatorParameters.h>
#include <TrackingTools/TrackAssociator/interface/TrackDetMatchInfo.h>
#include <TrackingTools/TrajectoryState/interface/FreeTrajectoryState.h>
#include <TrackPropagation/SteppingHelixPropagator/interface/SteppingHelixPropagator.h>
#include <TROOT.h>
#include <TTree.h>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <iterator>
#include <utility>

#include "../interface/FilterPlugin.h"

#include "TMultiGraph.h"
#include "TCanvas.h"

#include "CalibFormats/HcalObjects/interface/HcalDbService.h"

#include "CalibFormats/HcalObjects/interface/HcalDbRecord.h"

#include "DataFormats/HcalDigi/interface/HcalDigiCollections.h"
using namespace::std;

hoMuonAnalyzer::hoMuonAnalyzer(const edm::ParameterSet& iConfig)/*:
				assocParams(iConfig.getParameter<edm::ParameterSet>("TrackAssociatorParameters"))*/
{
	coutPrefix = "[hoMuonAnalyzer] ";
	//now do what ever initialization is needed

	//Get Input Tags from the Configuration

	_genInput = iConfig.getParameter<edm::InputTag>("genSrc");
	_l1MuonInput = iConfig.getParameter<edm::InputTag>("l1MuonSrc");
	_horecoInput = iConfig.getParameter<edm::InputTag>("horecoSrc");
	_l1MuonGenMatchInput = iConfig.getParameter<edm::InputTag>("l1MuonGenMatchSrc");
	_hltSumAODInput = iConfig.getParameter<edm::InputTag>("hltSumAODSrc");
	deltaR_Max = iConfig.getParameter<double>("maxDeltaR");
	threshold = iConfig.getParameter<double>("hoEnergyThreshold");
	debug = iConfig.getParameter<bool>("debug");
	deltaR_L1MuonMatching = iConfig.getParameter<double>("maxDeltaRL1MuonMatching");

	assoc.useDefaultPropagator();

	edm::ParameterSet parameters = iConfig.getParameter<edm::ParameterSet>("TrackAssociatorParameters");
	edm::ConsumesCollector iC = consumesCollector();
	assocParams.loadParameters( parameters, iC );

	singleMu3TrigName = "L1_SingleMuOpen";
	doubleMu0TrigName = "L1_DoubleMu_10_Open";
	doubleMu5TrigName = "L1_DoubleMu5 ";

	defineTriggersOfInterest();

	hoMatcher = new HoMatcher(iConfig);
	functionsHandler = new CommonFunctionsHandler(iConfig);

	/**
	 * Create the root tree for tuple storage. After that tell root to process the loader
	 * script which will provide support for the vectors of structs in the tree
	 */
	dataTree = _fileService->make<TTree>("dataTree","Tree with L1, Gen, and HO data");

	gROOT->ProcessLine(".L $CMSSW_BASE/src/HoMuonTrigger/loader.C+");

	l1MuonVector = new std::vector<L1MuonData>();
	genMuonVector = new std::vector<GenMuonData>();
	hoRecHitVector = new std::vector<HoRecHitData>();

	dataTree->Branch("l1MuonData","vector<L1MuonData>",l1MuonVector);
	dataTree->Branch("hoRecHitData","vector<HoRecHitData>",hoRecHitVector);
	dataTree->Branch("genMuonData","vector<GenMuonData>",genMuonVector);

	firstRun = true;

}


hoMuonAnalyzer::~hoMuonAnalyzer()
{
	delete hoMatcher;
	delete functionsHandler;
	hoMatcher = 0;
	functionsHandler = 0;
}

// ------------ method called for each event  ------------
void
hoMuonAnalyzer::analyze(const edm::Event& iEvent, 
		const edm::EventSetup& iSetup)
{
	/*
	 * Get Event Data and Event Setup
	 */

	if(firstRun){
		//for now we do not need the channel qualities any more
		//Uncomment again, if needed
//		printChannelQualities(iSetup);
		firstRun = false;
	}

	iEvent.getByLabel(_genInput,truthParticles);

	iEvent.getByLabel(_l1MuonInput, l1Muons);

	iEvent.getByLabel(_horecoInput, hoRecoHits);

	iSetup.get<CaloGeometryRecord>().get(caloGeo);

	iEvent.getByLabel(_l1MuonInput,l1MuonView);

	iEvent.getByLabel(_l1MuonGenMatchInput,l1MuonGenMatches);

	iEvent.getByLabel(edm::InputTag("muons"),recoMuons);

	edm::Handle<vector<PCaloHit>> caloHits;
	iEvent.getByLabel(edm::InputTag("g4SimHits","HcalHits"),caloHits);

	iSetup.get<IdealMagneticFieldRecord>().get(theMagField );

	hoMatcher->getEvent(iEvent,iSetup);
	functionsHandler->getEvent(iEvent);

	//Try getting the event info for weights
	edm::Handle<GenEventInfoProduct> genEventInfo;
	iEvent.getByLabel(edm::InputTag("generator"), genEventInfo);

	if (!caloHits.isValid()) {
		std::cout << coutPrefix << "no SimHits" << std::endl;
		return;
	}

	if (!MuonHOAcceptance::Inited()) MuonHOAcceptance::initIds(iSetup);

//	MuonHOAcceptance* mhoa = new MuonHOAcceptance();
//	std::cout << "Is inited: " << mhoa->Inited() << std::endl;
//	mhoa->initIds(iSetup);
//	TFile* f = new TFile("deadRegions.root","RECREATE");
//	TCanvas* c  = new TCanvas();
//	TMultiGraph* deadRegions = mhoa->graphDeadRegions();
//	deadRegions->Draw("ap");
//	c->Write();
//	f->Write();
//	f->Close();

	/**
	 * Loop over the collections for gen muons, l1muons and hoRechits
	 * Fill the information in vectors of structs and write this data to
	 * the root tree
	 */
	genMuonVector->clear();
	for( reco::GenParticleCollection::const_iterator genPart = truthParticles->begin() ; genPart != truthParticles->end(); genPart++){

		TrackDetMatchInfo * muMatch = getTrackDetMatchInfo(*genPart,iEvent,iSetup);
		double muMatchEta = muMatch->trkGlobPosAtHO.eta();
		double muMatchPhi = muMatch->trkGlobPosAtHO.phi();

		genMuonVector->push_back(GenMuonData(
				genPart->eta(),
				genPart->phi(),
				muMatchEta,
				muMatchPhi,
				genPart->pt(),
				genPart->pdgId(),
				(MuonHOAcceptance::inGeomAccept(genPart->eta(),genPart->phi()) && !hoMatcher->isInChimney(genPart->eta(),genPart->phi())),
				MuonHOAcceptance::inNotDeadGeom(genPart->eta(),genPart->phi()),
				MuonHOAcceptance::inSiPMGeom(genPart->eta(),genPart->phi())
		));

		if( MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi) && MuonHOAcceptance::inNotDeadGeom(muMatchEta,muMatchPhi) ){
			histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"tdmiInGaNotDead");
		}

		//The mu match position is inside HO acceptance
		if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi)
			&& MuonHOAcceptance::inNotDeadGeom(muMatchEta,muMatchPhi)
			&& !hoMatcher->isInChimney(muMatchEta,muMatchPhi)){

			histogramBuilder.fillCountHistogram("tdmiInGA");
			histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"tdmiInGA");

			const HORecHit* matchedRecHit = hoMatcher->matchByEMaxDeltaR(muMatch->trkGlobPosAtHO.eta(),muMatch->trkGlobPosAtHO.phi());
			//Found the Rec Hit with largest E
			if(matchedRecHit){
				double ho_eta = caloGeo->getPosition(matchedRecHit->id()).eta();
				double ho_phi = caloGeo->getPosition(matchedRecHit->id()).phi();
				histogramBuilder.fillDeltaEtaDeltaPhiHistograms(
						muMatch->trkGlobPosAtHO.eta(),
						ho_eta,
						muMatch->trkGlobPosAtHO.phi(),
						ho_phi,
						"tdmiHoMatch"
				);
				//Energy is above threshold
				if(matchedRecHit->energy() > threshold){
					histogramBuilder.fillDeltaEtaDeltaPhiHistograms(
							muMatch->trkGlobPosAtHO.eta(),
							ho_eta,
							muMatch->trkGlobPosAtHO.phi(),
							ho_phi,
							"tdmiHoAboveThr"
					);
					//TDMI has energy entry > 0
					if(muMatch->hoCrossedEnergy() > 0 ){
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(
								muMatch->trkGlobPosAtHO.eta(),
								ho_eta,
								muMatch->trkGlobPosAtHO.phi(),
								ho_phi,
								"tdmiHoAboveThrGt0"
						);
					} else{
						/**
						 * In this case, the muMatch from TDMI has 0 energy
						 * but was found to be in the geom acceptance
						 */
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(
								muMatch->trkGlobPosAtHO.eta(),
								ho_eta,
								muMatch->trkGlobPosAtHO.phi(),
								ho_phi,
								"tdmiHoAboveThrEq0"
						);
						histogramBuilder.fillEtaPhiGraph(
								muMatch->trkGlobPosAtHO.eta(),
								muMatch->trkGlobPosAtHO.phi(),
								"tdmiHoAboveThrEq0"
						);
					}
				}
			} else{
				/**
				 * There could not be found a rec hit by delta R matching
				 */
				histogramBuilder.fillCountHistogram("tdmiMatchHoFail");
				histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"tdmiMatchHoFail");
			}
		}//<-- TDMI in GA
	}

	l1MuonVector->clear();
	for( l1extra::L1MuonParticleCollection::const_iterator it = l1Muons->begin();
			it != l1Muons->end() ; it++ ){
		l1MuonVector->push_back(
				L1MuonData(
						it->eta(),
						it->phi(),
						it->pt(),
						it->bx(),
						(MuonHOAcceptance::inGeomAccept(it->eta(),it->phi() && !hoMatcher->isInChimney(it->eta(),it->phi()))),
						MuonHOAcceptance::inNotDeadGeom(it->eta(),it->phi()),
						MuonHOAcceptance::inSiPMGeom(it->eta(),it->phi())
				)
		);
	}

	hoRecHitVector->clear();
	for( auto it = hoRecoHits->begin(); it != hoRecoHits->end(); it++ ){
		histogramBuilder.fillMultiplicityHistogram(hoMatcher->countHoDigisByDetId(it->detid()),"hoDigiMatchesPerDetId");
		short* adcSamples = new short[10];
		const HODataFrame* dataFrame = hoMatcher->findHoDigiById(it->detid());
		for(int i = 0; i < std::min(10,dataFrame->size()); i++){
			adcSamples[i] = dataFrame->sample(i).adc();
		}
		hoRecHitVector->push_back(
				HoRecHitData(
						caloGeo->getPosition(it->id()).eta(),
						caloGeo->getPosition(it->id()).phi(),
						it->energy(),
						it->time(),
						adcSamples
				)
		);
		//Take care of memory! Adc samples are copied in the struct's constructor
		delete[] adcSamples;
	}

	dataTree->Fill();

	/**
	 * ###########################
	 * Done with filling the data into the trees
	 * ###########################
	 */

	/*
	 * Set Up Level 1 Global Trigger Utility
	 */

	bool useL1EventSetup = true;
	bool useL1GtTriggerMenuLite = true;

	m_l1GtUtils.getL1GtRunCache(iEvent, iSetup, useL1EventSetup,
			useL1GtTriggerMenuLite);
	/*
	 *
	 *  Start of Analysis
	 *
	 */

	histogramBuilder.fillCountHistogram("ProcessedEvents");

	//###############################
	// BEGIN Loop over Gen Particles only
	//###############################
	//Use this variable to store whether the event has GenMuons in acceptance
	bool hasMuonsInAcceptance = false;
	std::string gen_key = "gen";
	int genMuonCounter = 0;
	for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
			genIt != truthParticles->end(); genIt++){
		//Check for muons in Full barrel only
		if( ( abs(genIt->pdgId()) == 13 ) && ( abs(genIt->eta()) <= 0.8 ) ){
			hasMuonsInAcceptance = true;
			genMuonCounter++;
			histogramBuilder.fillPtHistogram(genIt->pt(),gen_key);
			histogramBuilder.fillEtaPhiGraph(genIt->eta(),genIt->phi(),gen_key);
			//For Ho geometric acceptance
			fillHoGeomAcceptanceGraph(*genIt);
			for (int i = 0; i < 200; i+=2) {
				if(genIt->pt() >= i){
					histogramBuilder.fillTrigRateHistograms(i,gen_key);
				}
			}
		}
	}
	histogramBuilder.fillMultiplicityHistogram(genMuonCounter,gen_key);
	//###############################
	// Loop over Gen Particles only DONE
	//###############################
	if(!hasMuonsInAcceptance){
		return;
	}
	histogramBuilder.fillCountHistogram("Events");
	analyzeWithGenLoop(iEvent,iSetup);

	/*
	 * Level 1 Muons
	 */
	string l1muon_key = "L1Muon";

	/**
	 * first loop over all L1 Muon objects. The contents of this Loop
	 * may be moved to the larger loop over l1 objects later in the code
	 */
	int successfulMatches = 0;
	int failedMatches = 0;
	//Define iterators
	l1extra::L1MuonParticleCollection::const_iterator bl1Muon = l1Muons->begin();
	l1extra::L1MuonParticleCollection::const_iterator el1Muon = l1Muons->end();
	for( unsigned int i = 0 ; i < l1Muons->size(); i++  ) {
		const l1extra::L1MuonParticle* bl1Muon = &(l1Muons->at(i));
		const reco::GenParticle* genMatch = getBestGenMatch(bl1Muon->eta(),bl1Muon->phi());
		if(genMatch){
			successfulMatches++;
			histogramBuilder.fillDeltaVzHistogam( (genMatch->vz() - bl1Muon->vz()) ,l1muon_key);
			histogramBuilder.fillCorrelationHistogram(genMatch->pt(),bl1Muon->pt(),"L1MuonPt");
			histogramBuilder.fillEtaPhiGraph(genMatch->eta(),genMatch->phi(),"L1ToGen");
			histogramBuilder.fillEtaPhiPtHistogram(genMatch->eta(), genMatch->phi(),genMatch->pt(),"L1ToGen");
			fillEfficiencyHistograms(bl1Muon->pt(),genMatch->pt(),"L1Muon");
			if(bl1Muon->gmtMuonCand().detector() == 2 /*DT only*/){
				histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),"BxDtOnly");
			}
			if(bl1Muon->bx() != 0){
				histogramBuilder.fillPtHistogram(genMatch->pt(),"BxWrongGen");
				histogramBuilder.fillEtaPhiGraph(genMatch->eta(),genMatch->phi(),"BxWrongGen");
				histogramBuilder.fillEtaPhiPtHistogram(genMatch->eta(), genMatch->phi(),genMatch->pt(),"BxWrongGen");
				/**
				 * Fill a multiplicity histogram with the detector index of the underlying GMT Cand
				 * From L1MuGmtExtendedCand:
				 * 1 RPC, 2 DT, 3 DT/RPC, 4 CSC, 5 CSC/RPC
				 *
				 * if (quality() == 7) // matched ?
					return isFwd() ? 5 : 3;
				   else
					return isRPC() ? 1 : ( isFwd()? 4 : 2);
				 */
				histogramBuilder.fillMultiplicityHistogram(bl1Muon->gmtMuonCand().detector(),"detectorIndexBxWrong");
			} else {
				histogramBuilder.fillPtHistogram(genMatch->pt(),"BxRightGen");
				histogramBuilder.fillEtaPhiGraph(genMatch->eta(),genMatch->phi(),"BxRightGen");
				histogramBuilder.fillEtaPhiPtHistogram(genMatch->eta(), genMatch->phi(),genMatch->pt(),"BxRightGen");
				histogramBuilder.fillMultiplicityHistogram(bl1Muon->gmtMuonCand().detector(),"detectorIndexBxRight");
			}
			/* Built this to fix the strange behaviour of the efficiency plots.
			 * Did not yet help completely. The reason for the strange behaviour is probably the fact,
			 * that there may be more than one l1 muons that can be matched to the Gen particle
			 */
			const HORecHit* matchedRecHit = hoMatcher->matchByEMaxDeltaR(bl1Muon->eta(),bl1Muon->phi());
			if(matchedRecHit){
				if(matchedRecHit->energy() > threshold)
					fillEfficiencyHistograms(bl1Muon->pt(),genMatch->pt(),"L1MuonAndHoAboveThr");
			}
		} else{
			failedMatches++;
		}
		edm::RefToBase<l1extra::L1MuonParticle> l1MuonCandiateRef(l1MuonView,i);
		reco::GenParticleRef ref = (*l1MuonGenMatches)[l1MuonCandiateRef];
		if(ref.isNonnull())
			histogramBuilder.fillPdgIdHistogram(ref->pdgId(),l1muon_key);
		else
			histogramBuilder.fillPdgIdHistogram(0,l1muon_key);
	}
	/**
	 * Count how often the matches to Gen were successful and how often they failed per Event.
	 * Also Count how often all matchings failed
	 */
	histogramBuilder.fillMultiplicityHistogram(failedMatches,"failedL1ToGenMatches");
	histogramBuilder.fillMultiplicityHistogram(successfulMatches,"successfulL1ToGenMatches");
	if(failedMatches && !successfulMatches){
		histogramBuilder.fillCountHistogram("allL1ToGenFailed");
	}
	//###############################
	// Loop over L1MuonObjects DONE
	//###############################

	//###############################
	// BEGIN Loop over HO Rec hits only
	//###############################
	string horeco_key = "horeco";
	string horecoT_key ="horecoAboveThreshold";
	/**
	 * Collect information of all HO Rec hits when there are
	 * l1 muons in the acceptance region
	 */
	int recHitAbThrCounter = 0;
	auto hoRecoIt = hoRecoHits->begin();
	for( ; hoRecoIt != hoRecoHits->end() ; hoRecoIt++){
		double ho_eta = caloGeo->getPosition(hoRecoIt->id()).eta();
		double ho_phi = caloGeo->getPosition(hoRecoIt->id()).phi();
		histogramBuilder.fillTimeHistogram(hoRecoIt->time(),"hoRecHits");
		if(hoRecoIt->energy() >= threshold){
			histogramBuilder.fillTimeHistogram(hoRecoIt->time(),"hoRecHitsAboveThr");
			histogramBuilder.fillEtaPhiHistograms(ho_eta, ho_phi,"hoRecHitsAboveThr");
			recHitAbThrCounter++;
		}
		histogramBuilder.fillCountHistogram(horeco_key);
		histogramBuilder.fillEnergyHistograms(hoRecoIt->energy(),horeco_key);
		histogramBuilder.fillEtaPhiHistograms(ho_eta, ho_phi, horeco_key);
	}
	histogramBuilder.fillMultiplicityHistogram(recHitAbThrCounter,horecoT_key);
	histogramBuilder.fillMultiplicityHistogram(hoRecoHits->size(),horeco_key);

	/*
	 * L1 Trigger Decisions
	 */
	singleMu3Trig = processTriggerDecision(singleMu3TrigName,iEvent);
	doubleMu0Trig = processTriggerDecision(doubleMu0TrigName,iEvent);
	//	processTriggerDecision(doubleMu5TrigName,iEvent);
	//###############################
	// Loop over HO Rec hits only DONE
	//###############################

	//###############################
	// Loop over L1 Muons and match to HO
	//###############################

	int countGenMatches = 0;
	string l1MuonWithHoMatch_key = "L1MuonWithHoMatch";
	bl1Muon = l1Muons->begin();
	el1Muon = l1Muons->end();
	histogramBuilder.fillMultiplicityHistogram(l1Muons->size(),"L1MuonPresent");
	if(l1Muons->size() > 0){
		histogramBuilder.fillCountHistogram("L1MuonPresent");
	}
	for( unsigned int i = 0 ; i < l1Muons->size(); i++ ){
		const l1extra::L1MuonParticle* bl1Muon = &(l1Muons->at(i));
		float l1Muon_eta = bl1Muon->eta();
		float l1Muon_phi = bl1Muon->phi();
		histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),"L1MuonPresent");
		histogramBuilder.fillBxIdVsPt(bl1Muon->bx(),bl1Muon->pt(),"L1MuonPresent");
		histogramBuilder.fillL1MuonPtHistograms(bl1Muon->pt(),"L1MuonPresent");
		histogramBuilder.fillPdgIdHistogram(bl1Muon->pdgId(),l1muon_key);
		histogramBuilder.fillVzHistogram(bl1Muon->vz(),l1muon_key);
		histogramBuilder.fillEtaPhiGraph(bl1Muon->eta(), bl1Muon->phi(), l1muon_key);
		//For variable binning
		listL1MuonPt.push_back(bl1Muon->pt());
		/*
		 * Fill histogram for different pt thresholds
		 * CAREFUL!! THIS IS NOT A REAL RATE YET!!
		 */
		for (int j = 0; j < 200; j+=2) {
			if(bl1Muon->pt() >= j){
				histogramBuilder.fillTrigRateHistograms(j,"L1MuonPresent");
			}
		}
		//Look for matches in grid around L1
		GlobalPoint l1Direction(bl1Muon->p4().X(),bl1Muon->p4().Y(),bl1Muon->p4().Z());
		fillGridMatchingEfficiency(l1Direction,bl1Muon->pt(),"L1Muon");

		if(MuonHOAcceptance::inGeomAccept(l1Muon_eta,l1Muon_phi)&& !hoMatcher->isInChimney(l1Muon_eta,l1Muon_phi)){
			histogramBuilder.fillCountHistogram("L1MuonInGA_L1Dir");
			if(bl1Muon->bx() == 0)
				histogramBuilder.fillCountHistogram("L1MuoninGaBx0");
			histogramBuilder.fillCorrelationGraph(l1Direction.eta(),l1Muon_eta,"Correlationp4AndL1Object");
		}

		const HORecHit* matchedRecHit = hoMatcher->matchByEMaxDeltaR(l1Muon_eta,l1Muon_phi);
		if(matchedRecHit){
			double hoEta,hoPhi;
			hoEta = caloGeo->getPosition(matchedRecHit->detid()).eta();
			hoPhi = caloGeo->getPosition(matchedRecHit->detid()).phi();
			histogramBuilder.fillCountHistogram("L1MuonPresentHoMatch");
			histogramBuilder.fillTimeHistogram(matchedRecHit->time(),"L1MuonPresentHoMatch");
			histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),"L1MuonPresentHoMatch");
			histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),"L1MuonPresentHoMatch");
			if (MuonHOAcceptance::inGeomAccept(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)&& !hoMatcher->isInChimney(l1Muon_eta,l1Muon_phi)){
				histogramBuilder.fillCountHistogram("L1MuonPresentHoMatchInAcc");
				histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),"L1MuonPresentHoMatchInAcc");

				//This energy check is done to test, whether the results depend on the order of the cuts applied
				//So far, the answer is no
				if(matchedRecHit->energy() >= threshold ){
					histogramBuilder.fillCountHistogram("L1MuonPresentHoMatchInAccThr");

				}

				if (MuonHOAcceptance::inNotDeadGeom(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
					histogramBuilder.fillCountHistogram("L1MuonPresentHoMatchInAccNotDead");
					histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),"L1MuonPresentHoMatchInAccNotDead");
					//This one is filled for the sake of completeness. The SiPM regions are hard-coded in the class!!
					if (MuonHOAcceptance::inSiPMGeom(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
						histogramBuilder.fillCountHistogram("L1MuonPresentHoMatchInAccNotDeadInSipm");
					}
				}
			}
			histogramBuilder.fillEnergyHistograms(matchedRecHit->energy(),l1MuonWithHoMatch_key);
			histogramBuilder.fillEtaPhiHistograms(hoEta, hoPhi,l1MuonWithHoMatch_key);
			histogramBuilder.fillDeltaEtaDeltaPhiHistograms(l1Muon_eta,hoEta,l1Muon_phi, hoPhi,l1MuonWithHoMatch_key);
			histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),l1MuonWithHoMatch_key);
			for (int i = 0; i < 200; i+=2) {
				if(bl1Muon->pt() >= i)
					histogramBuilder.fillTrigRateHistograms(i,"L1MuonWithHoNoThr");
			}
			const reco::GenParticle* bestGenMatch = getBestGenMatch(bl1Muon->eta(),bl1Muon->phi());
			if(bestGenMatch){
				countGenMatches++;
				fillEfficiencyHistograms(bl1Muon->pt(),bestGenMatch->pt(),"L1MuonHoReco");
			}

			//###########################################################
			//###########################################################
			//Now fill information for hits above threshold
			//###########################################################
			//###########################################################
			if(matchedRecHit->energy() >= threshold){
				//Fill some counting histograms. Can be used for cut flow in efficiency
				histogramBuilder.fillCountHistogram("L1MuonAboveThr");
				histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),"L1MuonAboveThr");
				histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),"L1MuonAboveThr");
				histogramBuilder.fillTimeHistogram(matchedRecHit->time(),"L1MuonAboveThr");
				histogramBuilder.fillBxIdVsPt(bl1Muon->bx(),bl1Muon->pt(),"L1MuonAboveThr");
				//Make time correlation plots depending on the different detector subsystems
				switch (bl1Muon->gmtMuonCand().detector()) {
					//RPC
					case 1:
						histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),"RpcHoAboveThr");
						break;
					//DT
					case 2:
						histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),"DtHoAboveThr");
						break;
					//RPC/DT
					case 3:
						histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),"RpcAndDtHoAboveThr");
						break;
					default:
						break;
				}
				double hoEta,hoPhi;
				hoEta = caloGeo->getPosition(matchedRecHit->detid()).eta();
				hoPhi = caloGeo->getPosition(matchedRecHit->detid()).phi();
				//Fill the HO information
				//Fill the counters
				if (MuonHOAcceptance::inGeomAccept(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)&& !hoMatcher->isInChimney(l1Muon_eta,l1Muon_phi)){
					histogramBuilder.fillCountHistogram("L1MuonAboveThrInAcc");
					histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),"L1MuonAboveThrInAcc");
					if (MuonHOAcceptance::inNotDeadGeom(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
						histogramBuilder.fillTimeHistogram(matchedRecHit->time(),"L1MuonAboveThrInAccNotDead");
						histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),"L1MuonAboveThrInAccNotDead");
						histogramBuilder.fillCountHistogram("L1MuonAboveThrInAccNotDead");
						histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),"L1MuonAboveThrInAccNotDead");
						histogramBuilder.fillTrigHistograms(caloGeo->present(matchedRecHit->id()),"caloGeoPresent_L1MuonHoMatchAboveThrFilt");
						histogramBuilder.fillEnergyHistograms(matchedRecHit->energy(),"L1MuonWithHoMatchAboveThrFilt");
						histogramBuilder.fillEtaPhiHistograms(hoEta,hoPhi,"L1MuonWithHoMatchAboveThrFilt_HO");
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(l1Muon_eta,hoEta,l1Muon_phi, hoPhi,"L1MuonWithHoMatchAboveThrFilt");
						histogramBuilder.fillL1MuonPtHistograms(bl1Muon->pt(),"L1MuonWithHoMatchAboveThrFilt");
						histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),"L1MuonWithHoMatchAboveThrFilt");
						//Make time correlation plots depending on the different detector subsystems
						//This time for HO in GA only
						switch (bl1Muon->gmtMuonCand().detector()) {
							//RPC
							case 1:
								histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),"RpcHoAboveThrFilt");
								break;
							//DT
							case 2:
								histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),"DtHoAboveThrFilt");
								break;
							//RPC/DT
							case 3:
								histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),"RpcAndDtHoAboveThrFilt");
								break;
							default:
								break;
						}
						const reco::GenParticle* bestGenMatch = getBestGenMatch(bl1Muon->eta(),bl1Muon->phi());
						if(bestGenMatch){
							fillEfficiencyHistograms(bl1Muon->pt(),bestGenMatch->pt(),"L1MuonHoRecoAboveThrFilt");
						}

						//This one is filled for the sake of completeness. The SiPM regions are hardcoded in the class!!
						if (MuonHOAcceptance::inSiPMGeom(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
							histogramBuilder.fillCountHistogram("L1MuonAboveThrInAccNotDeadInSipm");
						}
					}
				}
				histogramBuilder.fillEnergyHistograms(matchedRecHit->energy(),"L1MuonWithHoMatchAboveThr");
				histogramBuilder.fillEtaPhiHistograms(hoEta,hoPhi,"L1MuonWithHoMatchAboveThr_HO");
				histogramBuilder.fillDeltaEtaDeltaPhiHistograms(l1Muon_eta,hoEta,l1Muon_phi, hoPhi,"L1MuonWithHoMatchAboveThr");
				histogramBuilder.fillL1MuonPtHistograms(bl1Muon->pt(),"L1MuonWithHoMatchAboveThr");
				histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),"L1MuonWithHoMatchAboveThr");
				//Make the pseudo trig rate plot
				for (int i = 0; i < 200; i+=2) {
					if(bl1Muon->pt() >= i)
						histogramBuilder.fillTrigRateHistograms(i,l1MuonWithHoMatch_key);
				}
				const reco::GenParticle* bestGenMatch = getBestGenMatch(bl1Muon->eta(),bl1Muon->phi());
				if(bestGenMatch){
					fillEfficiencyHistograms(bl1Muon->pt(),bestGenMatch->pt(),"L1MuonHoRecoAboveThr");
					histogramBuilder.fillBxIdVsPt(bl1Muon->bx(),bl1Muon->pt(),"L1MuonAboveThrGenMatch");
				}
				//Try to find a corresponding Gen Muon
				edm::RefToBase<l1extra::L1MuonParticle> l1MuonCandiateRef(l1MuonView,i);
				reco::GenParticleRef ref = (*l1MuonGenMatches)[l1MuonCandiateRef];

				if(ref.isNonnull()){
					string l1MuonAndHoRecoAndGenref_key = "L1MuonandHORecowithMipMatchAndGenMatch";
					//Make the pseudo trig rate plot
					for (int i = 0; i < 200; i+=2) {
						if(bl1Muon->pt() >= i)
							histogramBuilder.fillTrigRateHistograms(i,l1MuonAndHoRecoAndGenref_key);
					}
					histogramBuilder.fillPdgIdHistogram(ref->pdgId(),l1MuonWithHoMatch_key);
				} else{
					histogramBuilder.fillPdgIdHistogram(0,l1MuonWithHoMatch_key);
				}
				histogramBuilder.fillL1MuonPtHistograms(bl1Muon->pt(), l1MuonWithHoMatch_key);
				histogramBuilder.fillEtaPhiGraph(bl1Muon->eta(), bl1Muon->phi(), "L1MuonWithHoMatchAboveThr_L1Mu");
			}// E > thr.
		}
	}//<-- For loop over all l1muons
	histogramBuilder.fillMultiplicityHistogram(countGenMatches,"nL1WithGenMatch");
	//################################
	//################################
	//		This is for the case where no L1Muon was found
	//################################
	//################################
	if(l1Muons->size() == 0){
		histogramBuilder.fillCountHistogram("NoL1Muon");
		int recHitAbThrNoL1Counter = 0;
		auto hoRecoIt = hoRecoHits->begin();
		for( ; hoRecoIt != hoRecoHits->end() ; hoRecoIt++){
			if(hoRecoIt->energy() >= threshold){
				recHitAbThrNoL1Counter++;
				double ho_eta = caloGeo->getPosition(hoRecoIt->id()).eta();
				double ho_phi = caloGeo->getPosition(hoRecoIt->id()).phi();
				histogramBuilder.fillCountHistogram(horeco_key);
				histogramBuilder.fillEnergyHistograms(hoRecoIt->energy(),"NoL1");
				histogramBuilder.fillEtaPhiHistograms(ho_eta, ho_phi, "NoL1");
				histogramBuilder.fillEnergyVsPosition(ho_eta,ho_phi,hoRecoIt->energy(),"NoL1");
			}
		}
		histogramBuilder.fillMultiplicityHistogram(recHitAbThrNoL1Counter,"NoL1");

		for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
				genIt != truthParticles->end(); genIt++){
			//Check for muons in Full barrel only
			//Try to find a corresponding Gen Muon
			float genEta = genIt->eta();
			float genPhi = genIt->phi();

			/**
			 * #############################################
			 * # Use TrackDetMatchInfo to see where the gen particle ends up in HO. When selecting only events, where gen is in
			 * # the geometric acceptance of HO, this gives a more realistic estimator for the number of recoverable
			 * # Triggers
			 * #############################################
			 */
			TrackDetMatchInfo * muMatch = getTrackDetMatchInfo(*genIt,iEvent,iSetup);

			double muMatchPhi = muMatch->trkGlobPosAtHO.phi();
			double muMatchEta = muMatch->trkGlobPosAtHO.eta();
			histogramBuilder.fillEtaPhiGraph(genEta,genPhi,"NoL1GenAny");
			histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoL1TdmiAny");
			if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi)
				&& MuonHOAcceptance::inNotDeadGeom(muMatchEta,muMatchPhi)
				&& !hoMatcher->isInChimney(muMatchEta,muMatchPhi)
			){
				histogramBuilder.fillEtaPhiGraph(genEta,genPhi,"NoL1GenInGA");
				histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoL1TdmiInGA");
			}
		}

	}// <-- l1muons size == 0
	else{
		/**
		 * #################################
		 * # L1 Muon objects contain data
		 * # FIXME: Loop over l1 and try to find gens. This way, the direction information of the strange l1 is already available
		 * # Loop over gens and inspect also the case, where the matching from gen to l1 failed
		 * # This might be a hint on ghost reduction
		 * #################################
		 */
		for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
				genIt != truthParticles->end(); genIt++){
			float genEta = genIt->eta();
			float genPhi = genIt->phi();

			TrackDetMatchInfo * muMatch = getTrackDetMatchInfo(*genIt,iEvent,iSetup);
			double muMatchPhi = muMatch->trkGlobPosAtHO.phi();
			double muMatchEta = muMatch->trkGlobPosAtHO.eta();

			if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi)&& !hoMatcher->isInChimney(muMatchEta,muMatchPhi)){
				histogramBuilder.fillCountHistogram("TdmiInGA_TdmiDir");
				std::vector<const HORecHit*> crossedHoRecHits = muMatch->crossedHORecHits;
				GlobalPoint muMatchDirection(
						muMatch->trkGlobPosAtHO.X(),
						muMatch->trkGlobPosAtHO.Y(),
						muMatch->trkGlobPosAtHO.Z()
				);
				fillGridMatchingEfficiency(muMatchDirection,genIt->pt(),"tdmi");
			}
			const l1extra::L1MuonParticle* l1Part = functionsHandler->getBestL1MuonMatch(muMatchEta,muMatchPhi);
			if(l1Part){
				double deltaEta = muMatchEta - l1Part->eta();
				double deltaPhi = FilterPlugin::wrapCheck(muMatchPhi, l1Part->phi());
				histogramBuilder.fillGraph(deltaEta,deltaPhi,"deltaEtaDeltaPhiTdmiL1");
			}
			const HORecHit* matchedRecHit = hoMatcher->matchByEMaxDeltaR(genEta,genPhi);
			if(matchedRecHit){
				double hoEta = caloGeo->getPosition(matchedRecHit->id()).eta();
				double hoPhi = caloGeo->getPosition(matchedRecHit->id()).phi();
				histogramBuilder.fillDeltaEtaDeltaPhiEnergyHistogram(genEta,hoEta,genPhi,hoPhi,matchedRecHit->energy(),"WithSingleMu");
				if(matchedRecHit->energy() >= threshold){
					histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,"WithSingleMuAboveThr");
					if (MuonHOAcceptance::inGeomAccept(genEta,genPhi/*,deltaR_Max,deltaR_Max*/)&& !hoMatcher->isInChimney(genEta,genPhi)){
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,"WithSingleMuGeomAcc");
						if (MuonHOAcceptance::inNotDeadGeom(genEta,genPhi/*,deltaR_Max,deltaR_Max*/)){
							histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,"WithSingleMuNotDead");
						}
					}
				}
			}//Matched rec hit
		}//Gen particle loop
	}
	//#############################################################
	//#############################################################
	// NO SINGLE MU TRIGGER
	//#############################################################
	//#############################################################


	if(!singleMu3Trig){
		histogramBuilder.fillMultiplicityHistogram(l1Muons->size(),"NoSingleMu_L1Muon");
		analyzeNoSingleMuEventsL1Loop(iEvent,iSetup);
		analyzeNoSingleMuEventsGenLoop(iEvent,iSetup);
		//################################
		//################################
		// Match Ho to gen info and try to recover mu trigger
		//################################
		//################################
		for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
				genIt != truthParticles->end(); genIt++){
			//Check for muons in Full barrel only
			//Try to find a corresponding Gen Muon
			float genEta = genIt->eta();
			float genPhi = genIt->phi();

			/**
			 * #############################################
			 * # Use TrackDetMatchInfo to see where the gen particle ends up in HO. When selecting only events, where gen is in
			 * # the geometric acceptance of HO, this gives a more realistic estimator for the number of recoverable
			 * # Triggers
			 * #############################################
			 */
			TrackDetMatchInfo * muMatch = getTrackDetMatchInfo(*genIt,iEvent,iSetup);
			double muMatchPhi = muMatch->trkGlobPosAtHO.phi();
			double muMatchEta = muMatch->trkGlobPosAtHO.eta();

			//Studies depending on whether there are L1 Objects or not
			if(l1Muons->size()>0){
				histogramBuilder.fillCountHistogram("NoTriggerButL1Muons");
			}
			else{
				histogramBuilder.fillCountHistogram("NoTrgNoL1Any");
				histogramBuilder.fillEtaPhiGraph(genEta,genPhi,"NoTrgNoL1GenAny");
				histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoTrgNoL1TdmiAny");
				if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi) && !hoMatcher->isInChimney(muMatchEta,muMatchPhi)){
					histogramBuilder.fillEtaPhiGraph(genEta,genPhi,"NoTrgNoL1GenInGA");
					histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoTrgNoL1TdmiInGA");
				}
			}

			histogramBuilder.fillEtaPhiGraph(genEta,genPhi,"NoTrgGenAny");
			histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoTrgTdmiAny");
			//The muon needs to hit the HO geometric acceptance
			if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi) && !hoMatcher->isInChimney(muMatchEta,muMatchPhi)){
				histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoTrgTdmiInGA");
				histogramBuilder.fillCountHistogram("NoTrgTdmiInGA");
				histogramBuilder.fillEnergyVsPosition(muMatchEta,muMatchPhi,muMatch->hoCrossedEnergy(),"NoTrgTdmiXedE");
				const HORecHit* matchedRecHit = hoMatcher->matchByEMaxDeltaR(muMatchEta,muMatchPhi);
				//Where is the Rec hit in a delta R cone with the largest E?
				//Did we find any?
				if(matchedRecHit){
					histogramBuilder.fillCountHistogram("NoTrgTdmiInGAHoMatch");
					double hoEta = caloGeo->getPosition(matchedRecHit->id()).eta();
					double hoPhi = caloGeo->getPosition(matchedRecHit->id()).phi();
					histogramBuilder.fillDeltaEtaDeltaPhiHistograms(muMatchEta,hoEta,muMatchPhi,hoPhi,"NoTrgTdmi");
					//Apply energy cut on the matched RecHit
					if(matchedRecHit->energy() >= threshold ){
						histogramBuilder.fillCountHistogram("NoTrgTdmiInGAHoAboveThr");
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(muMatchEta,hoEta,muMatchPhi,hoPhi,"NoTrgTdmiAboveThr");
						histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoTrgTdmiAboveThr");
						histogramBuilder.fillEtaPhiPtHistogram(muMatchEta,muMatchPhi,genIt->pt(),"NoTrgTdmiAboveThr");
						histogramBuilder.fillEtaPhiGraph(hoEta,hoPhi,"NoTrgTdmiAboveThrHoCoords");
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,"NoTrgGenAboveThr");
						histogramBuilder.fillEnergyVsPosition(muMatchEta,muMatchPhi,muMatch->hoCrossedEnergy(),"NoTrgTdmiAboveThrXedE");
						histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),"NoTrgTdmiAboveThrHoE");
						if( (muMatchEta > -0.35 && muMatchEta < -0.185) || (muMatchEta > 0.16 && muMatchEta < 0.3) ){
							if( (muMatchPhi > 0.7 && muMatchPhi < 1.36) || (muMatchPhi > 1.2 && muMatchPhi < 1.9) ){
								ofstream myfile;
								myfile.open ("eventNumbers.txt",std::ios::app);
								myfile << iEvent.id().event() << std::endl;
							}
						}
					//inspect the crossed energy, when the matched Rec hit in the cone was below threshold
					} else{
						histogramBuilder.fillEnergyVsPosition(muMatchEta,muMatchPhi,muMatch->hoCrossedEnergy(),"NoTrgTdmiBelowThrXedE");
						histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoTrgTdmiBelowThr");
						histogramBuilder.fillEtaPhiPtHistogram(muMatchEta,muMatchPhi,genIt->pt(),"NoTrgTdmiBelowThr");
					}
				//Count the events, where we could not match a Rec hit in the delta dR cone
				} else{
					histogramBuilder.fillCountHistogram("NoTrgHoMatchFail");
					histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoTrgHoMatchFail");
					histogramBuilder.fillEtaPhiPtHistogram(muMatchEta,muMatchPhi,genIt->pt(),"NoTrgHoMatchFail");
				}
			}//<-- in GA
			else{
				histogramBuilder.fillEtaPhiGraph(muMatchEta,muMatchPhi,"NoTrgTdmiNotInGA");
				histogramBuilder.fillEtaPhiPtHistogram(muMatchEta,muMatchPhi,genIt->pt(),"NoTrgTdmiNotInGA");
			}
		}//Loop over gen particles
	}//<-- Not single mu trg
	//#############################################################
	//#############################################################
	// SINGLE MU TRIGGER FIRED
	//#############################################################
	//#############################################################
	else {
		/**
		 * #######################
		 * ######################
		 * The following part serves for checking cases where there is a L1 Muon trigger
		 * It's possible that some code is similar or the same as above. But for clarity reasons
		 * the code below is used
		 * #######################
		 * ######################
		 */
		analyzeL1MuonsForGhosts(iEvent,iSetup);
		for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
				genIt != truthParticles->end(); genIt++){
			float genEta = genIt->eta();
			float genPhi = genIt->phi();

			TrackDetMatchInfo * muMatch = getTrackDetMatchInfo(*genIt,iEvent,iSetup);
			double muMatchPhi = muMatch->trkGlobPosAtHO.phi();
			double muMatchEta = muMatch->trkGlobPosAtHO.eta();

			//Require the muon to hit the HO area
			if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi) && !hoMatcher->isInChimney(muMatchEta,muMatchPhi)){

				histogramBuilder.fillCountHistogram("SMuTrgTdmiInGA");
				const l1extra::L1MuonParticle* l1Ref = functionsHandler->getBestL1MuonMatch(genEta,genPhi);
				if(l1Ref){
					histogramBuilder.fillCountHistogram("SMuTrgFoundL1Match");
					float l1Muon_eta = l1Ref->eta();
					float l1Muon_phi = l1Ref->phi();
					fillEfficiencyHistograms(l1Ref->pt(),genIt->pt(),"SMuTrgL1AndGenMatch");
					const HORecHit* matchedRecHit = hoMatcher->matchByEMaxDeltaR(l1Muon_eta,l1Muon_phi);
					//Check whether an HO match could be found by the delta R method
					if(matchedRecHit){
						histogramBuilder.fillCountHistogram("SMuTrgL1AndFoundHoMatch");
						//inspect the data where HO was above the threshold
						if(matchedRecHit->energy() >= threshold){
							histogramBuilder.fillCountHistogram("SMuTrgL1AndHoAboveThr");
							fillEfficiencyHistograms(l1Ref->pt(),genIt->pt(),"SMuTrgL1AndGenMatchHoAboveThr");
						}
					}
				}//<-- Found L1 ref
				else{
					const HORecHit* matchedRecHit = hoMatcher->matchByEMaxDeltaR(muMatchEta,muMatchPhi);
					//Check whether an HO match could be found by the delta R method
					if(matchedRecHit){
						histogramBuilder.fillCountHistogram("SMuTrgAndFoundHoMatch");
						//inspect the data where HO was above the threshold
						if(matchedRecHit->energy() >= threshold){
							histogramBuilder.fillCountHistogram("SMuTrgAndHoAboveThr");
						}
					}
				}
			}
		}//Gen loop


		//Loop over L1 Objects
		//##################################################
		//##################################################
		// L1 Muons for the "Oliver Style" efficiency
		//##################################################
		//##################################################
		for( unsigned int i = 0 ; i < l1Muons->size(); i++  ) {
			const l1extra::L1MuonParticle* bl1Muon = &(l1Muons->at(i));
			double l1Muon_eta = bl1Muon->eta();
			double l1Muon_phi = bl1Muon->phi();
			if(MuonHOAcceptance::inGeomAccept(l1Muon_eta,l1Muon_phi)&& !hoMatcher->isInChimney(l1Muon_eta,l1Muon_phi)){
				histogramBuilder.fillCountHistogram("L1MuonSMuTrgInGA_L1Dir");
				GlobalPoint l1Direction(
						bl1Muon->p4().X(),
						bl1Muon->p4().Y(),
						bl1Muon->p4().Z()
				);
				if(hoMatcher->hasHoHitInGrid(l1Direction,0)){
					histogramBuilder.fillCountHistogram("L1MuonSMuTrgCentral");
				}
				if(hoMatcher->hasHoHitInGrid(l1Direction,1)){
					histogramBuilder.fillCountHistogram("L1MuonSMuTrg3x3");
				}
			}
		}
	}//<-- End of Single mu trg
}
//#############################
// End of Analyze function
//#############################


/**
 * Small helper function to print the number of triggers for a certain algorithm name
 */
bool hoMuonAnalyzer::processTriggerDecision(std::string algorithmName,const edm::Event& iEvent){
	// Select on events that pass a specific L1Trigger Decision
	int iErrorCode = -1;
	bool trigDecision = m_l1GtUtils.decision(iEvent, algorithmName, iErrorCode);
	if(iErrorCode == 0){
		histogramBuilder.fillTrigHistograms(trigDecision,algorithmName);
		if(trigDecision){
			histogramBuilder.fillCountHistogram(algorithmName);
		}

	} else if (iErrorCode == 1) {
		cout<< coutPrefix << "trigger " << algorithmName << " does not exist in the L1 menu" << endl;
	} else {
		// error - see error code
		cout << coutPrefix << "Error Code " << iErrorCode << std::endl;
		cout << coutPrefix << "Algorithm name: " << algorithmName << std::endl;
	}
	return trigDecision;
}


// ------------ method called once each job just before starting event loop  ------------
void 
hoMuonAnalyzer::beginJob()
{
}

// ------------ method called once each job just after ending the event loop  ------------
void 
hoMuonAnalyzer::endJob() 
{
}

// ------------ method called when starting to processes a run  ------------

void 
hoMuonAnalyzer::beginRun(const edm::Run& iRun, 
		const edm::EventSetup& evSetup)
{

	bool useL1EventSetup = true;
	bool useL1GtTriggerMenuLite = true;
	std::cout << coutPrefix << "getL1GtRunCache" << std::endl;
	std::cout << coutPrefix << "UseL1EventSetup: " << useL1EventSetup << std::endl;
	std::cout << coutPrefix << "UseL1GtTriggerMenuLite: " << useL1GtTriggerMenuLite << std::endl;
	m_l1GtUtils.getL1GtRunCache(iRun, evSetup, useL1EventSetup, useL1GtTriggerMenuLite);

}


// ------------ method called when ending the processing of a run  ------------

void 
hoMuonAnalyzer::endRun(const edm::Run& iRun, const edm::EventSetup& evSetup)
{

	//Only interested in unique values
	listL1MuonPt.sort();
	listL1MuonPt.unique();  //NB it is called after sort
	if(debug){
		std::cout << coutPrefix << "The list contains " << listL1MuonPt.size() << "unique entries:";
		std::list<float>::iterator it;
		for (it=listL1MuonPt.begin(); it!=listL1MuonPt.end(); ++it){
			std::cout << ' ' << *it;
		}
		std::cout << endl;
	}
}


// ------------ method fills 'descriptions' with the allowed parameters for the module  ------------
void
hoMuonAnalyzer::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
	//The following says we do not know what parameters are allowed so do no validation
	// Please change this to state exactly what you do use, even if it is no parameters
	edm::ParameterSetDescription desc;
	desc.setUnknown();
	descriptions.addDefault(desc);
}

/*############################
 * Helper Functions
 *############################
 */
//Look for an L1 object in the direction of the HLT object
const l1extra::L1MuonParticle* hoMuonAnalyzer::getMatchedL1Object(trigger::TriggerObject hltObject
		,edm::Handle<l1extra::L1MuonParticleCollection> l1muons){
	double hltPhi,hltEta;
	double l1Phi,l1Eta;
	hltEta = hltObject.eta();
	hltPhi = hltObject.phi();
	for(unsigned int i = 0; i < l1muons->size(); i++){
		const l1extra::L1MuonParticle* l1muon = &(l1muons->at(i));
		l1Eta = l1muon->eta();
		l1Phi = l1muon->phi();
		if(FilterPlugin::isInsideDeltaR(hltEta,l1Eta,hltPhi,l1Phi,deltaR_Max))
			return l1muon;
	}
	return NULL;
}

/**
 * Returns a pointer to the closest gen particle of all particles that are closer
 * than delta R < delta R max
 */
const reco::GenParticle* hoMuonAnalyzer::getBestGenMatch(float eta, float phi){
	const reco::GenParticle* bestGen = 0;
	float bestDR = 999.;
	reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
	reco::GenParticleCollection::const_iterator genEnd = truthParticles->end();
	for(; genIt!=genEnd; ++genIt) {
		if (abs(genIt->pdgId()) == 13 ) {
			float genPhi = genIt->phi();
			float genEta = genIt->eta();
			float dR = deltaR(eta,phi,genEta,genPhi);
			if (dR < deltaR_L1MuonMatching && dR < bestDR) { // CB get it from CFG
				bestDR = dR;
				bestGen = &(*genIt);
			}
		}
	}
	return bestGen;
}

void hoMuonAnalyzer::defineTriggersOfInterest(){

	/*
	 * HLT Triggers
	 */

	string hltIsoMu24_key = "hltIsoMu24";
	hltNamesOfInterest.insert(pair<string, string>(hltIsoMu24_key,"HLT_IsoMu24_v18"));
	hltFiltersOfInterest.insert(pair<string, edm::InputTag>(hltIsoMu24_key,
			edm::InputTag("hltL3crIsoL1sMu16L1f0L2f16QL3"
					"f24QL3crIsoRhoFiltered0p15",
					"","HLT")));

	string hltMu5_key = "hltMu5";
	hltNamesOfInterest.insert(pair<string, string>(hltMu5_key, "HLT_Mu5_v21"));
	hltFiltersOfInterest.insert(pair<string, edm::InputTag>(hltMu5_key,
			edm::InputTag("hltL3fL1sMu3L3Filtered5",
					"","HLT")));

	string l1SingleMuOpen_key = "hlt_l1SingleMuOpen";
	hltNamesOfInterest.insert(std::pair<std::string,std::string>(l1SingleMuOpen_key,"HLT_L1SingleMuOpen_v7"));
	hltFiltersOfInterest.insert(std::pair<std::string,edm::InputTag>(l1SingleMuOpen_key,edm::InputTag("hltL1MuOpenL1Filtered0","","HLT")));

}

/**
 * Gets the TrackDetMatchInfo for a Gen Particle by using its vertex, momentum and charge information
 * Needs the magnetic field, the edm::event and the edm::setup of an event
 */
TrackDetMatchInfo* hoMuonAnalyzer::getTrackDetMatchInfo(reco::GenParticle genPart,const edm::Event& iEvent,
		const edm::EventSetup& iSetup){
	//Create the Track det match info
	GlobalPoint vertexPoint(genPart.vertex().X(),genPart.vertex().Y(),genPart.vertex().Z());
	GlobalVector mom (genPart.momentum().x(),genPart.momentum().y(),genPart.momentum().z());
	int charge = genPart.charge();
	const FreeTrajectoryState *freetrajectorystate_ = new FreeTrajectoryState(vertexPoint, mom ,charge , &(*theMagField));
	return new TrackDetMatchInfo(assoc.associate(iEvent, iSetup, *freetrajectorystate_, assocParams));
}

/**
 * Automatically call the histogram builder to fill the efficiency objects with different cut thresholds
 * Needs the pt for evaluation of the efficiency (ptMeasured) and the real pT
 */
void hoMuonAnalyzer::fillEfficiencyHistograms(double ptMeasured, double ptReal,std::string key){
	histogramBuilder.fillEfficiency(ptMeasured >= 5, ptReal, key + "Pt5");
	histogramBuilder.fillEfficiency(ptMeasured >= 10, ptReal, key + "Pt10");
	histogramBuilder.fillEfficiency(ptMeasured >= 15, ptReal, key + "Pt15");
	histogramBuilder.fillEfficiency(ptMeasured >= 20, ptReal, key + "Pt20");
	histogramBuilder.fillEfficiency(ptMeasured >= 25, ptReal, key + "Pt25");
}

/**
 * Prints all channel qualities for HO into histogram which itself is stored
 * in a root file
 */
void hoMuonAnalyzer::printChannelQualities(const edm::EventSetup& iSetup){
	std::cout << coutPrefix << "Printing Channel qualities" << std::endl;
	edm::ESHandle<HcalChannelQuality> p;
	iSetup.get<HcalChannelQualityRcd>().get(p);
	HcalChannelQuality *myqual = new HcalChannelQuality(*p.product());
	edm::ESHandle<HcalSeverityLevelComputer> mycomputer;
	iSetup.get<HcalSeverityLevelComputerRcd>().get(mycomputer);
	const HcalSeverityLevelComputer *mySeverity = mycomputer.product();
	int ieta, iphi;
	TFile* channelStatusfile = new TFile("channelStatus.root","RECREATE");
	TH2D* channelStatusHist = new TH2D("channelStatusHist","Channel status of HO",34,-16.5,16.5,73,-0.5,73.5);
	for (ieta=-15; ieta <= 15; ieta++) {
		if (ieta != 0) {
			for (iphi = 1; iphi <= 72; iphi++) {
				HcalDetId did(HcalOuter,ieta,iphi,4);
				const HcalChannelStatus *mystatus = myqual->getValues(did.rawId());
				channelStatusHist->SetBinContent(channelStatusHist->FindBin(ieta,iphi),mystatus->getValue());
				if (mySeverity->dropChannel(mystatus->getValue())) {
				}
			}
		}
	}
	channelStatusHist->Write();
	channelStatusfile->Write();
	channelStatusfile->Close();
}

/**
 * This function analyzes the information in the collection produced by the l1muon and gen matcher
 */
void hoMuonAnalyzer::analyzeL1AndGenMatch(const edm::Event& iEvent, const edm::EventSetup& iSetup){
	for(unsigned int i = 0; i < l1Muons->size() ; i++){
		const l1extra::L1MuonParticle* l1Muon = &(l1Muons->at(i));
		edm::RefToBase<l1extra::L1MuonParticle> l1MuonCandiateRef(l1MuonView,i);
		reco::GenParticleRef ref = (*l1MuonGenMatches)[l1MuonCandiateRef];
		if(ref.isNonnull()){
			histogramBuilder.fillEfficiency(true,l1Muon->pt(),"L1GenRef");
			//Once there is a gen ref, get the Track det match info
			TrackDetMatchInfo * muMatch = getTrackDetMatchInfo(*ref,iEvent,iSetup);
			double muMatchEta = muMatch->trkGlobPosAtHO.eta();
			double muMatchPhi = muMatch->trkGlobPosAtHO.phi();
			histogramBuilder.fillCountHistogram("L1GenRef");
			if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi)
						&& MuonHOAcceptance::inNotDeadGeom(muMatchEta,muMatchPhi)
						&& !hoMatcher->isInChimney(muMatchEta,muMatchPhi)){
				histogramBuilder.fillCountHistogram("L1GenRefInGa");
				GlobalPoint l1Direction(
						l1Muon->p4().X(),
						l1Muon->p4().Y(),
						l1Muon->p4().Z()
						);
				fillGridMatchingEfficiency(l1Direction,ref->pt(),"L1GenRefInGa",muMatchEta,muMatchPhi);
			}

		} else {
			histogramBuilder.fillEfficiency(false,l1Muon->pt(),"L1GenRef");
			histogramBuilder.fillEtaPhiGraph(l1Muon->eta(),l1Muon->phi(),"L1GenRefFail");
		}
	}
}

/**
 * Analyzer function for events with no single muon trigger.
 * Tries to find l1 muons and match them to ho information
 */
void hoMuonAnalyzer::analyzeNoSingleMuEventsL1Loop(const edm::Event& iEvent,const edm::EventSetup& iSetup){
	for(unsigned int i = 0; i < l1Muons->size() ; i++){
		const l1extra::L1MuonParticle* l1Muon = &(l1Muons->at(i));
		edm::RefToBase<l1extra::L1MuonParticle> l1MuonCandiateRef(l1MuonView,i);
		reco::GenParticleRef ref = (*l1MuonGenMatches)[l1MuonCandiateRef];
		if(ref.isNonnull()){
			histogramBuilder.fillEfficiency(true,l1Muon->pt(),"L1GenRefNoSingleMu");
			//Once there is a gen ref, get the Track det match info
			TrackDetMatchInfo * muMatch = getTrackDetMatchInfo(*ref,iEvent,iSetup);
			double muMatchEta = muMatch->trkGlobPosAtHO.eta();
			double muMatchPhi = muMatch->trkGlobPosAtHO.phi();
			histogramBuilder.fillCountHistogram("L1GenRefNoSingleMu");
			if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi)
			&& MuonHOAcceptance::inNotDeadGeom(muMatchEta,muMatchPhi)
			&& !hoMatcher->isInChimney(muMatchEta,muMatchPhi)){
				histogramBuilder.fillCountHistogram("L1GenRefNoSingleMuInGa");
				GlobalPoint l1Direction(
						l1Muon->p4().X(),
						l1Muon->p4().Y(),
						l1Muon->p4().Z()
				);
				fillGridMatchingEfficiency(l1Direction, ref->pt(),"L1GenRefNoSingleMuInGa",muMatchEta,muMatchPhi);
			}

		} else {
			histogramBuilder.fillEfficiency(false,l1Muon->pt(),"L1GenRefNoSingleMu");
			histogramBuilder.fillEtaPhiGraph(l1Muon->eta(),l1Muon->phi(),"L1GenRefNoSingleMuFail");
		}
	}
}

/**
 * Analyzer function for events with no single muon trigger.
 * loops over gen muons and match them to ho information
 */
void hoMuonAnalyzer::analyzeNoSingleMuEventsGenLoop(const edm::Event& iEvent,const edm::EventSetup& iSetup){
	for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
			genIt != truthParticles->end(); genIt++){

		histogramBuilder.fillPtHistogram(genIt->pt(),"NoSingleMu");

		//Once there is a gen ref, get the Track det match info
		TrackDetMatchInfo * muMatch = getTrackDetMatchInfo(*genIt,iEvent,iSetup);
		double muMatchEta = muMatch->trkGlobPosAtHO.eta();
		double muMatchPhi = muMatch->trkGlobPosAtHO.phi();
		histogramBuilder.fillCountHistogram("NoSingleMu");
		if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi)
		&& MuonHOAcceptance::inNotDeadGeom(muMatchEta,muMatchPhi)
		&& !hoMatcher->isInChimney(muMatchEta,muMatchPhi)){
			histogramBuilder.fillCountHistogram("NoSingleMuInGa");
			GlobalPoint genDirection(
					genIt->p4().X(),
					genIt->p4().Y(),
					genIt->p4().Z()
			);
			fillGridMatchingEfficiency(genDirection, genIt->pt(),"NoSingleMuInGa",muMatchEta,muMatchPhi);
		}
	}
}

/**
 * Use this function to make the efficiency plots with root's TEfficiency.
 */
void hoMuonAnalyzer::analyzeWithGenLoop(const edm::Event& iEvent,const edm::EventSetup& iSetup){
	for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
			genIt != truthParticles->end(); genIt++){
		float genEta = genIt->eta();
		float genPhi = genIt->phi();
		const l1extra::L1MuonParticle* l1Part = 0;
		l1Part = functionsHandler->getBestL1MuonMatch(genEta,genPhi);
		histogramBuilder.fillCountHistogram("Gen");
		if(l1Part){
			fillEfficiencyHistograms(l1Part->pt(),genIt->pt(),"GenAndL1Muon");
			histogramBuilder.fillCountHistogram("GenAndL1Muon");
			GlobalPoint l1Direction(
					l1Part->p4().X(),
					l1Part->p4().Y(),
					l1Part->p4().Z()
			);
			fillGridMatchingEfficiency(l1Direction,l1Part->pt(),"L1MuonTruth");
			fillAverageEnergyAroundL1Direction(l1Part);
			/**
			 * Find a rec hit that can be matched to the l1 particle. Use this information for the efficiency
			 * plots. This time it is ensured that only as many entries as there are gen particles is used
			 * This fixes double counting of ghost l1 muons
			 */
			const HORecHit* matchedRecHit = 0;
			matchedRecHit = hoMatcher->matchByEMaxDeltaR(l1Part->eta(),l1Part->phi());
			if(matchedRecHit){
				if(matchedRecHit->energy() > threshold){
					fillEfficiencyHistograms(l1Part->pt(),genIt->pt(),"GenAndL1MuonAndHoAboveThr");
					histogramBuilder.fillCountHistogram("GenAndL1MuonAndHoAboveThr");

				}
			}
		}
	}
}

/**
 * Fill a histogram with the measured energy around a given L1.
 * For now the grid size is hard-coded
 */
void hoMuonAnalyzer::fillAverageEnergyAroundL1Direction(const l1extra::L1MuonParticle* l1Muon){
	int gridSize = 5;
	for(auto recHitIt = hoRecoHits->begin(); recHitIt != hoRecoHits->end(); recHitIt++){
		if(hoMatcher->isRecHitInGrid(l1Muon->eta(), l1Muon->phi(),&*recHitIt,gridSize)){
			histogramBuilder.fillDeltaEtaDeltaPhiHistogramsWithWeights(l1Muon->eta()
					,float(hoMatcher->getRecHitEta(&*recHitIt))	,l1Muon->phi()
					,float(hoMatcher->getRecHitPhi(&*recHitIt))	,recHitIt->energy()
					,"averageEnergyAroundPoint");
			histogramBuilder.fillDeltaEtaDeltaPhiEnergyHistogram(l1Muon->eta()
					,float(hoMatcher->getRecHitEta(&*recHitIt))	,l1Muon->phi()
					,float(hoMatcher->getRecHitPhi(&*recHitIt))	,recHitIt->energy()
					,"averageEnergyAroundPoint");//Use this function for the 1D distributions for each delta eta and delta phi
		}
	}

	//Filling the average energy only for the highest energetic particle
	const HORecHit* matchedRecHit = 0;
	matchedRecHit = hoMatcher->findEMaxHitInGrid(l1Muon->eta(), l1Muon->phi(),5);
	if(matchedRecHit){
		histogramBuilder.fillDeltaEtaDeltaPhiHistogramsWithWeights(l1Muon->eta()
				,float(hoMatcher->getRecHitEta(matchedRecHit))	,l1Muon->phi()
				,float(hoMatcher->getRecHitPhi(matchedRecHit))	,matchedRecHit->energy()
				,"averageEMaxAroundPoint");
		histogramBuilder.fillDeltaEtaDeltaPhiEnergyHistogram(l1Muon->eta()
				,float(hoMatcher->getRecHitEta(matchedRecHit))	,l1Muon->phi()
				,float(hoMatcher->getRecHitPhi(matchedRecHit))	,matchedRecHit->energy()
				,"averageEMaxAroundPoint");
	}
}

/**
 * Fills an eta phi graph in case the gen particle is in the geometric
 * acceptance of HO.
 * Use a function for this to keep the code clean
 */
void hoMuonAnalyzer::fillHoGeomAcceptanceGraph(reco::GenParticle genPart){
	if(MuonHOAcceptance::inGeomAccept(genPart.eta(),genPart.phi())
				&& MuonHOAcceptance::inNotDeadGeom(genPart.eta(),genPart.phi())
				&& !hoMatcher->isInChimney(genPart.eta(),genPart.phi())){
		histogramBuilder.fillEtaPhiGraph(genPart.eta(),genPart.phi(),"HoGeomAcceptance");
	}
}

/**
 * Automatically fill efficiency and count histograms for the grid matching for grid sizes
 * central, 3x3 and 5x5. Also store the position information
 * #FIXME: The getDetIdsCloseToAPoint behaves strangely. Swith to a function, that evaluates whether
 * a given rec hit coordinate is inside a grid or not
 */
void hoMuonAnalyzer::fillGridMatchingEfficiency(GlobalPoint direction, float pt, std::string key, float eta, float phi){
	//#####
	// Central tile
	//#####
	if(hoMatcher->hasHoHitInGrid(direction,0)){
		histogramBuilder.fillCountHistogram(key + "Central");
		histogramBuilder.fillEfficiency(true,pt,key + "Central");
		histogramBuilder.fillEtaPhiGraph(eta,phi,key + "Central");
	} else{
		histogramBuilder.fillEfficiency(false,pt,key + "Central");
		histogramBuilder.fillEtaPhiGraph(eta,phi,key + "CentralFail");
	}
	//#####
	// 3 x 3
	//#####
	if(hoMatcher->hasHoHitInGrid(direction,1)){
		histogramBuilder.fillCountHistogram(key + "3x3");
		histogramBuilder.fillEfficiency(true,pt,key + "3x3");
		histogramBuilder.fillEtaPhiGraph(eta,phi,key + "3x3");

	} else {
		histogramBuilder.fillEfficiency(false,pt,key + "3x3");
		histogramBuilder.fillEtaPhiGraph(eta,phi,key + "3x3Fail");
	}
	//#####
	// 5 x 5
	//#####
	if(hoMatcher->hasHoHitInGrid(direction,2)){
		histogramBuilder.fillCountHistogram(key + "5x5");
		histogramBuilder.fillEfficiency(true,pt,key + "5x5");
		histogramBuilder.fillEtaPhiGraph(eta,phi,key + "5x5");


	} else {
		histogramBuilder.fillEfficiency(false,pt,key + "5x5");
		histogramBuilder.fillEtaPhiGraph(eta,phi,key + "5x5Fail");
	}
}

/**
 * Automatically fill efficiency and count histograms for the grid matching for grid sizes
 * central, 3x3 and 5x5. The position information is always 0 if this function is used
 */
void hoMuonAnalyzer::fillGridMatchingEfficiency(GlobalPoint direction, float pt, std::string key){
	fillGridMatchingEfficiency(direction,pt,key,0,0);
}

/**
 * Perform studies on ghost using reco information.
 * Loop over L1 Muons and try to match them to recos and HO
 */
void hoMuonAnalyzer::analyzeL1MuonsForGhosts(const edm::Event& iEvent,const edm::EventSetup& iSetup){
//	/**
//	 * Use the L1 objects as seeds for ghost searches
//	 */
//
//	typedef std::pair< const l1extra::L1MuonParticle*,double > L1DeltaRPair;
//	typedef std::pair< double,const reco::GenParticle*> DeltaRGenPair;
//	typedef std::pair< const l1extra::L1MuonParticle*,DeltaRGenPair> L1Match;
//
//	typedef std::map< const l1extra::L1MuonParticle*,DeltaRGenPair> L1MatchingMap;
//
//	std::vector<const reco::GenParticle*> gensToBeMatched;
//	std::vector<const l1extra::L1MuonParticle*> availableL1;
//	L1MatchingMap matchingMap;
//
//	for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
//			genIt != truthParticles->end(); genIt++){
//		gensToBeMatched.push_back(&(*genIt));
//	}
//
//	for(l1extra::L1MuonParticleCollection::const_iterator l1It = l1Muons->begin();
//			l1It != l1Muons->end(); l1It++){
//		availableL1.push_back(&(*l1It));
//		matchingMap[&(*l1It)] = DeltaRGenPair(999.,0);
//	}
//
//	L1Match* bestDeltaR = 0;
//
//	for( std::vector<const reco::GenParticle*>::const_iterator gen = gensToBeMatched.begin() ;
//			gen != gensToBeMatched.end() ; gen++ ){
//		double bestDeltaR = 999.;
//		const l1extra::L1MuonParticle* l1ref;
//		for ( std::vector<const l1extra::L1MuonParticle*>::const_iterator l1 = availableL1.begin() ;
//				l1 != availableL1.end() ; l1++ ){
//			double newDeltaR = deltaR((*gen)->eta(),(*gen)->phi(),(*l1)->eta(),(*l1)->phi());
//			if( newDeltaR < bestDeltaR ){
//				if( newDeltaR < matchingMap[*l1].first ){
//					bestDeltaR = newDeltaR;
//					l1ref = *l1ref;
//				}
//			}
//		}
//		if( matchingMap[l1ref].second != 0 ){
//			gensToBeMatched.push_back(matchingMap[l1ref].second);
//		}
//		matchingMap[l1ref] = DeltaRGenPair(bestDeltaR,*gen);
//
//	}
//
//
//	L1GenPair* bestCombination;
//	for(unsigned int i = 0; i < l1Muons->size() ; i++){
//		const l1extra::L1MuonParticle* l1Muon = &(l1Muons->at(i));
//		double dR = 999;
//		const reco::Muon* lastStaMuon = 0;
//		const reco::Muon* globalMuon = 0;
//		/**
//		 * Loop over all reco muons and find the best delta R match. Save refs to global
//		 * muons and STA muons if there are any within a given delta R cone
//		 */
//		const reco::Muon* muon = 0;
//		for ( unsigned int i = 0 ; i < recoMuons->size() ; i++) {
//			muon = &(recoMuons->at(i));
//			double newDeltaR = deltaR(l1Muon->eta(),l1Muon->phi(),muon->eta(),muon->phi());
//			if( newDeltaR < threshold && newDeltaR < dR ){
//				dR = newDeltaR;
//				if( muon->isGlobalMuon() ){
//					globalMuon = muon;
//				} else {
//					if( muon->isStandAloneMuon() ){
//						lastStaMuon = &(*muon);
//					}
//				}
//			}
//		}
//		//After the reco loop start inspecting the ghost cases
//		if(globalMuon){
//			//If there is a global muon, assume that this is not a ghost
//			continue;
//		} else{
////			GlobalPoint l1Direction(
////					l1Muon->p4().X(),
////					l1Muon->p4().Y(),
////					l1Muon->p4().Z()
////			);
//			if(lastStaMuon){
//				//TODO: Fill in code that helps analyzing the cases where there is only a STA Muon
//				//This is probably not a ghost scenario!
//			} else{
//
//			}
//		}
//	}
}

//define this as a plug-in
DEFINE_FWK_MODULE(hoMuonAnalyzer);
