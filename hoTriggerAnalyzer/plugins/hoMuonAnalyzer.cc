// -*- C++ -*-
//
// Package:    caloInspector
// Class:      caloInspector
// 
/**\class caloInspector caloInspector.cc L1TriggerDPGUpgrade/caloInspector/plugins/caloInspector.cc

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
#include "../interface/HoMatcher.h"

using namespace::std;

hoMuonAnalyzer::hoMuonAnalyzer(const edm::ParameterSet& iConfig)/*:
				assocParams(iConfig.getParameter<edm::ParameterSet>("TrackAssociatorParameters"))*/
{
	coutPrefix = std::string("[hoMuonAnalyzer] ");
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

	singleMu3TrigName = "L1_SingleMu3";
	doubleMu0TrigName = "L1_DoubleMu0";
	doubleMu5TrigName = "L1_DoubleMu5 ";

	defineTriggersOfInterest();


	/**
	 * Create the root tree for tuple storage. After that tell root to process the loader
	 * script which will provide support for the vetors of structs in the tree
	 */
	dataTree = _fileService->make<TTree>("dataTree","Tree with L1, Gen, and HO data");

	gROOT->ProcessLine(".L ./loader.C+");

	l1MuonVector = new std::vector<L1MuonData>();
	genMuonVector = new std::vector<GenMuonData>();
	hoRecHitVector = new std::vector<HoRecHitData>();

	dataTree->Branch("l1MuonData","vector<L1MuonData>",l1MuonVector);
	dataTree->Branch("hoRecHitData","vector<HoRecHitData>",hoRecHitVector);
	dataTree->Branch("genMuonData","vector<GenMuonData>",genMuonVector);

}


hoMuonAnalyzer::~hoMuonAnalyzer()
{

	// do anything here that needs to be done at desctruction time
	// (e.g. close files, deallocate resources etc.)

	//f->Close();

}


//
// member functions#include "DataFormats/GeometryVector/interface/GlobalTag.h"

//

// ------------ method called for each event  ------------
void
hoMuonAnalyzer::analyze(const edm::Event& iEvent, 
		const edm::EventSetup& iSetup)
{

	if (!MuonHOAcceptance::Inited()) MuonHOAcceptance::initIds(iSetup);

	/*
	 * Get Event Data and Event Setup
	 */

	iEvent.getByLabel(_genInput,truthParticles);

	iEvent.getByLabel(_l1MuonInput, l1Muons);

	edm::Handle<HORecHitCollection> hoRecoHits;
	iEvent.getByLabel(_horecoInput, hoRecoHits);

	edm::ESHandle<CaloGeometry> caloGeo;
	iSetup.get<CaloGeometryRecord>().get(caloGeo);

	edm::Handle<edm::View<l1extra::L1MuonParticle> > l1MuonView;
	iEvent.getByLabel(_l1MuonInput,l1MuonView);

	edm::Handle<reco::GenParticleMatch> l1MuonGenMatches;
	iEvent.getByLabel(_l1MuonGenMatchInput,l1MuonGenMatches);

	edm::Handle<vector<PCaloHit>> caloHits;
	iEvent.getByLabel(edm::InputTag("g4SimHits","HcalHits"),caloHits);

	if (!caloHits.isValid()) {
		std::cout << coutPrefix << "no SimHits" << std::endl;
		return;
	}

	edm::ESHandle<Propagator> shProp;
//	iSetup.get<TrackingComponentsRecord>().get("SteppingHelixPropagatorAny", shProp);

	edm::ESHandle<MagneticField> theMagField;
	iSetup.get<IdealMagneticFieldRecord>().get(theMagField );
	SteppingHelixPropagator myHelix(&*theMagField,anyDirection);

	//Generate the energy correlation plot
	for(HORecHitCollection::const_iterator recHitIt = hoRecoHits->begin();
			recHitIt != hoRecoHits->end(); recHitIt++){
		for(std::vector<PCaloHit>::const_iterator caloHitIt = caloHits->begin();
				caloHitIt != caloHits->end(); caloHitIt++){
			HcalDetId tempId(caloHitIt->id());
			if(tempId.subdet() == HcalSubdetector::HcalOuter){
				if(tempId == recHitIt->id() ){
					histogramBuilder.fillEnergyCorrelationHistogram(caloHitIt->energy(),recHitIt->energy(),std::string("energyCorr"));

				}
			}
		}
	}

	/**
	 * Loop over the collections for gen muons, l1muons and hoRechits
	 * Fill the information in vectors of structs and write this data to
	 * the root tree
	 */
	genMuonVector->clear();
	for( reco::GenParticleCollection::const_iterator genPart = truthParticles->begin() ; genPart != truthParticles->end(); genPart++){
		genMuonVector->push_back(GenMuonData(
				genPart->eta(),
				genPart->phi(),
				genPart->pt(),
				genPart->pdgId(),
				MuonHOAcceptance::inGeomAccept(genPart->eta(),genPart->phi()),
				MuonHOAcceptance::inNotDeadGeom(genPart->eta(),genPart->phi()),
				MuonHOAcceptance::inSiPMGeom(genPart->eta(),genPart->phi())
		));

		GlobalPoint vertexPoint(genPart->vertex().X(),genPart->vertex().Y(),genPart->vertex().Z());
		GlobalVector mom (genPart->momentum().x(),genPart->momentum().y(),genPart->momentum().z());
		int charge = genPart->charge();
		const FreeTrajectoryState *freetrajectorystate_ =
		new FreeTrajectoryState(vertexPoint, mom ,charge , &(*theMagField));
		TrackDetMatchInfo * muMatch = new TrackDetMatchInfo(assoc.associate(iEvent, iSetup, *freetrajectorystate_, assocParams));

		double muMatchEta = 0;
		double muMatchPhi = 0;

		muMatchEta = muMatch->trkGlobPosAtHO.eta();
		muMatchPhi = muMatch->trkGlobPosAtHO.phi();

		const HORecHit* matchedRecHit = HoMatcher::matchByEMaxDeltaR(muMatch->trkGlobPosAtHO.eta(),muMatch->trkGlobPosAtHO.phi(),deltaR_Max,*hoRecoHits,*caloGeo);
		//Found the Rec Hit with largest E
		if(matchedRecHit){
			double ho_eta = caloGeo->getPosition(matchedRecHit->id()).eta();
			double ho_phi = caloGeo->getPosition(matchedRecHit->id()).phi();

			histogramBuilder.fillDeltaEtaDeltaPhiHistograms(
				muMatch->trkGlobPosAtHO.eta(),
				ho_eta,
				muMatch->trkGlobPosAtHO.phi(),
				ho_phi,
				std::string("tdmiHo")
			);
			//The mu match position is inside HO acceptance
			if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi)){
				histogramBuilder.fillDeltaEtaDeltaPhiHistograms(
						muMatch->trkGlobPosAtHO.eta(),
						ho_eta,
						muMatch->trkGlobPosAtHO.phi(),
						ho_phi,
						std::string("tdmiInGeomAcc")
				);
				//Energy is above threshold
				if(matchedRecHit->energy() > threshold){
					histogramBuilder.fillDeltaEtaDeltaPhiHistograms(
							muMatch->trkGlobPosAtHO.eta(),
							ho_eta,
							muMatch->trkGlobPosAtHO.phi(),
							ho_phi,
							std::string("tdmiHoAboveThr")
					);
					//TDMI has energy entry > 0
					if(muMatch->hoCrossedEnergy() > 0 ){
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(
								muMatch->trkGlobPosAtHO.eta(),
								ho_eta,
								muMatch->trkGlobPosAtHO.phi(),
								ho_phi,
								std::string("tdmiHoAboveThrGt0")
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
								std::string("tdmiHoAboveThrEq0")
						);
						histogramBuilder.fillEtaPhiHistograms(
								muMatch->trkGlobPosAtHO.eta(),
								muMatch->trkGlobPosAtHO.phi(),
								std::string("tdmiHoAboveThrEq0")
						);
					}
				}
			}
		} else{
			/**
			 * There could not be found a rec hit by delta R matching
			 */
			histogramBuilder.fillCountHistogram(std::string("tdmiMatchHoFail"));
			histogramBuilder.fillEtaPhiHistograms(muMatchEta,muMatchPhi,std::string("tdmiMatchHoFail"));
		}
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
						MuonHOAcceptance::inGeomAccept(it->eta(),it->phi()),
						MuonHOAcceptance::inNotDeadGeom(it->eta(),it->phi()),
						MuonHOAcceptance::inSiPMGeom(it->eta(),it->phi())
				)
		);
	}

	hoRecHitVector->clear();
	for( auto it = hoRecoHits->begin(); it != hoRecoHits->end(); it++ ){
		hoRecHitVector->push_back(
				HoRecHitData(
						caloGeo->getPosition(it->id()).eta(),
						caloGeo->getPosition(it->id()).phi(),
						it->energy(),
						it->time()
				)
		);
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


	//Try getting the event info for weights
	edm::Handle<GenEventInfoProduct> genEventInfo;
	iEvent.getByLabel(edm::InputTag("generator"), genEventInfo);

	/*
	 * Fill a trig rate histogramm for the muons of the gen particles
	 */
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
			histogramBuilder.fillEtaPhiHistograms(genIt->eta(),genIt->phi(),gen_key);
			for (int i = 0; i < 200; i+=2) {
				if(genIt->pt() >= i){
					histogramBuilder.fillTrigRateHistograms(i,gen_key);
				}
			}
		}
	}
	histogramBuilder.fillMultiplicityHistogram(genMuonCounter,gen_key);

	if(!hasMuonsInAcceptance)
		return;

	histogramBuilder.fillCountHistogram("Events");
	/*
	 * Level 1 Muons
	 */
	string l1muon_key = "L1Muon";
	histogramBuilder.fillMultiplicityHistogram(l1Muons->size(),l1muon_key);


	//Define iterators
	l1extra::L1MuonParticleCollection::const_iterator bl1Muon = l1Muons->begin();
	l1extra::L1MuonParticleCollection::const_iterator el1Muon = l1Muons->end();

	for( unsigned int i = 0 ; i < l1Muons->size(); i++  ) {
		histogramBuilder.fillCountHistogram(l1muon_key);
		const l1extra::L1MuonParticle* bl1Muon = &(l1Muons->at(i));
//		ofstream myfile;
//		myfile.open ("L1MuonPt.txt",std::ios::app);
//		myfile << bl1Muon->pt() << std::endl;
//		myfile.close();
		histogramBuilder.fillPdgIdHistogram(bl1Muon->pdgId(),l1muon_key);
		histogramBuilder.fillVzHistogram(bl1Muon->vz(),l1muon_key);
		const reco::GenParticle* genMatch = getBestGenMatch(bl1Muon->eta(),bl1Muon->phi());
		if(genMatch){
			histogramBuilder.fillDeltaVzHistogam( (genMatch->vz() - bl1Muon->vz()) ,l1muon_key);
			histogramBuilder.fillPtCorrelationHistogram(genMatch->pt(),bl1Muon->pt(),l1muon_key);
		}
		/*
		 * Fill histogram for different pt thresholds
		 * CAREFUL!! THIS IS NOT A REAL RATE YET!!
		 */
		for (int j = 0; j < 200; j+=2) {
			if(bl1Muon->pt() >= j){
				histogramBuilder.fillTrigRateHistograms(j,l1muon_key);
			}
		}

		const reco::GenParticle* bestGenMatch = getBestGenMatch(bl1Muon->eta(),bl1Muon->phi());
		if(bestGenMatch){
			//first argument is the condition for a muon trigger object to pass
			//Second is the pt of the "real" particle
			histogramBuilder.fillEfficiency(bl1Muon->pt()>=5,bestGenMatch->pt(),std::string("L1MuonPt5"));
			histogramBuilder.fillEfficiency(bl1Muon->pt()>=10,bestGenMatch->pt(),std::string("L1MuonPt10"));
			histogramBuilder.fillEfficiency(bl1Muon->pt()>=15,bestGenMatch->pt(),std::string("L1MuonPt15"));
			histogramBuilder.fillEfficiency(bl1Muon->pt()>=20,bestGenMatch->pt(),std::string("L1MuonPt20"));
			histogramBuilder.fillEfficiency(bl1Muon->pt()>=25,bestGenMatch->pt(),std::string("L1MuonPt25"));
		}
		histogramBuilder.fillL1MuonPtHistograms(bl1Muon->pt(), l1muon_key);
		histogramBuilder.fillEtaPhiHistograms(bl1Muon->eta(), bl1Muon->phi(),
				l1muon_key);
		//For variable binning
		listL1MuonPt.push_back(bl1Muon->pt());
		edm::RefToBase<l1extra::L1MuonParticle> l1MuonCandiateRef(l1MuonView,i);
		reco::GenParticleRef ref = (*l1MuonGenMatches)[l1MuonCandiateRef];
		if(ref.isNonnull())
			histogramBuilder.fillPdgIdHistogram(ref->pdgId(),l1muon_key);
		else
			histogramBuilder.fillPdgIdHistogram(0,l1muon_key);
	}



	/*
	 * Multiplicities
	 */
	string horeco_key = "horeco";
	/**
	 * Fill the multiplicity histograms for HORecHits without cuts
	 */
	histogramBuilder.fillMultiplicityHistogram(hoRecoHits->size(),horeco_key);

	//get HO Rec Hits Above Threshold
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
		histogramBuilder.fillTimeHistogram(hoRecoIt->time(),std::string("hoRecHits"));
		if(hoRecoIt->energy() >= threshold){
			histogramBuilder.fillTimeHistogram(hoRecoIt->time(),std::string("hoRecHitsAboveThr"));
			histogramBuilder.fillEtaPhiHistograms(ho_eta, ho_phi, std::string("hoRecHitsAboveThr"));
			recHitAbThrCounter++;
		}
		histogramBuilder.fillCountHistogram(horeco_key);
		histogramBuilder.fillEnergyHistograms(hoRecoIt->energy(),horeco_key);
		histogramBuilder.fillEtaPhiHistograms(ho_eta, ho_phi, horeco_key);
	}
	histogramBuilder.fillMultiplicityHistogram(recHitAbThrCounter,horecoT_key);


	/*
	 * L1 Trigger Decisions
	 */
	singleMu3Trig = processTriggerDecision(singleMu3TrigName,iEvent);
	doubleMu0Trig = processTriggerDecision(doubleMu0TrigName,iEvent);
	//	processTriggerDecision(doubleMu5TrigName,iEvent);


	/*
	 * L1 Muons and matched HO information
	 */
	int countGenMatches = 0;
	string l1MuonWithHoMatch_key = "L1MuonWithHoMatch";
	bl1Muon = l1Muons->begin();
	el1Muon = l1Muons->end();

	for( unsigned int i = 0 ; i < l1Muons->size(); i++ ){

		const l1extra::L1MuonParticle* bl1Muon = &(l1Muons->at(i));

		//first fill information for ho hits without energy threshold
		//###########################################################
		float l1Muon_eta = bl1Muon->eta();
		float l1Muon_phi = bl1Muon->phi();
		histogramBuilder.fillCountHistogram(std::string("L1MuonPresent"));
		histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),std::string("L1MuonPresent"));
		histogramBuilder.fillL1MuonPtHistograms(bl1Muon->pt(),std::string("L1MuonPresent"));
		const HORecHit* matchedRecHit = HoMatcher::matchByEMaxDeltaR(l1Muon_eta,l1Muon_phi,deltaR_Max,*hoRecoHits,*caloGeo);
		if(matchedRecHit){
			double hoEta,hoPhi;
			hoEta = caloGeo->getPosition(matchedRecHit->detid()).eta();
			hoPhi = caloGeo->getPosition(matchedRecHit->detid()).phi();
			histogramBuilder.fillCountHistogram("L1MuonPresentHoMatch");
			histogramBuilder.fillTimeHistogram(matchedRecHit->time(),std::string("L1MuonPresentHoMatch"));
			histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),std::string("L1MuonPresentHoMatch"));
			histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),std::string("L1MuonPresentHoMatch"));
			if (MuonHOAcceptance::inGeomAccept(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
				histogramBuilder.fillCountHistogram(std::string("L1MuonPresentHoMatchInAcc"));
				histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),std::string("L1MuonPresentHoMatchInAcc"));

				//This energy check is done to test, whether the results depend on the order of the cuts applied
				//So far, the answer is no
				if(matchedRecHit->energy() >= threshold ){
					histogramBuilder.fillCountHistogram(std::string("L1MuonPresentHoMatchInAccThr"));
				}

				if (MuonHOAcceptance::inNotDeadGeom(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
					histogramBuilder.fillCountHistogram(std::string("L1MuonPresentHoMatchInAccNotDead"));
					histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),std::string("L1MuonPresentHoMatchInAccNotDead"));
					//This one is filled for the sake of completeness. The SiPM regions are hard-coded in the class!!
					if (MuonHOAcceptance::inSiPMGeom(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
						histogramBuilder.fillCountHistogram(std::string("L1MuonPresentHoMatchInAccNotDeadInSipm"));
					}
				}
			}
			histogramBuilder.fillEnergyHistograms(matchedRecHit->energy(),l1MuonWithHoMatch_key);
			histogramBuilder.fillEtaPhiHistograms(hoEta, hoPhi,l1MuonWithHoMatch_key);
			histogramBuilder.fillDeltaEtaDeltaPhiHistograms(l1Muon_eta,hoEta,l1Muon_phi, hoPhi,l1MuonWithHoMatch_key);
			histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),l1MuonWithHoMatch_key);
			for (int i = 0; i < 200; i+=2) {
				if(bl1Muon->pt() >= i)
					histogramBuilder.fillTrigRateHistograms(i,std::string("L1MuonWithHoNoThr"));
			}
			const reco::GenParticle* bestGenMatch = getBestGenMatch(bl1Muon->eta(),bl1Muon->phi());
			if(bestGenMatch){
				countGenMatches++;
				//first argument is the condition for a muon trigger object to pass
				//Second is the pt of the "real" particle
				histogramBuilder.fillEfficiency(bl1Muon->pt()>=5,bestGenMatch->pt(),std::string("L1MuonPt5HoReco"));
				histogramBuilder.fillEfficiency(bl1Muon->pt()>=10,bestGenMatch->pt(),std::string("L1MuonPt10HoReco"));
				histogramBuilder.fillEfficiency(bl1Muon->pt()>=15,bestGenMatch->pt(),std::string("L1MuonPt15HoReco"));
				histogramBuilder.fillEfficiency(bl1Muon->pt()>=20,bestGenMatch->pt(),std::string("L1MuonPt20HoReco"));
				histogramBuilder.fillEfficiency(bl1Muon->pt()>=25,bestGenMatch->pt(),std::string("L1MuonPt25HoReco"));
			}

			//###########################################################
			//###########################################################
			//Now fill information for hits above threshold
			//###########################################################
			//###########################################################
			if(matchedRecHit->energy() >= threshold){
				//Fill some counting histograms. Can be used for cut flow in efficiency
				histogramBuilder.fillCountHistogram(std::string("L1MuonAboveThr"));
				histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),std::string("L1MuonAboveThr"));
				histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),std::string("L1MuonAboveThr"));
				histogramBuilder.fillTimeHistogram(matchedRecHit->time(),std::string("L1MuonAboveThr"));
				double hoEta,hoPhi;
				hoEta = caloGeo->getPosition(matchedRecHit->detid()).eta();
				hoPhi = caloGeo->getPosition(matchedRecHit->detid()).phi();
				//Fill the HO information
				//Fill the counters
				if (MuonHOAcceptance::inGeomAccept(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
					histogramBuilder.fillCountHistogram(std::string("L1MuonAboveThrInAcc"));
					histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),std::string("L1MuonAboveThrInAcc"));
					if (MuonHOAcceptance::inNotDeadGeom(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
						histogramBuilder.fillTimeHistogram(matchedRecHit->time(),std::string("L1MuonAboveThrInAccNotDead"));
						histogramBuilder.fillDeltaTimeHistogram(matchedRecHit->time(),bl1Muon->bx(),std::string("L1MuonAboveThrInAccNotDead"));
						histogramBuilder.fillCountHistogram(std::string("L1MuonAboveThrInAccNotDead"));
						histogramBuilder.fillBxIdHistogram(bl1Muon->bx(),std::string("L1MuonAboveThrInAccNotDead"));
						histogramBuilder.fillTrigHistograms(caloGeo->present(matchedRecHit->id()),std::string("caloGeoPresent_L1MuonHoMatchAboveThrFilt"));
						histogramBuilder.fillEnergyHistograms(matchedRecHit->energy(),std::string("L1MuonWithHoMatchAboveThrFilt"));
						histogramBuilder.fillEtaPhiHistograms(hoEta,hoPhi,std::string("L1MuonWithHoMatchAboveThrFilt_HO"));
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(l1Muon_eta,hoEta,l1Muon_phi, hoPhi,std::string("L1MuonWithHoMatchAboveThrFilt"));
						histogramBuilder.fillL1MuonPtHistograms(bl1Muon->pt(),std::string("L1MuonWithHoMatchAboveThrFilt"));
						histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),std::string("L1MuonWithHoMatchAboveThrFilt"));

						const reco::GenParticle* bestGenMatch = getBestGenMatch(bl1Muon->eta(),bl1Muon->phi());
						if(bestGenMatch){
							//first argument is the condition for a muon trigger object to pass
							//Second is the pt of the "real" particle
							histogramBuilder.fillEfficiency(bl1Muon->pt()>=5,bestGenMatch->pt(),std::string("L1MuonPt5HoRecoAboveThrFilt"));
							histogramBuilder.fillEfficiency(bl1Muon->pt()>=10,bestGenMatch->pt(),std::string("L1MuonPt10HoRecoAboveThrFilt"));
							histogramBuilder.fillEfficiency(bl1Muon->pt()>=15,bestGenMatch->pt(),std::string("L1MuonPt15HoRecoAboveThrFilt"));
							histogramBuilder.fillEfficiency(bl1Muon->pt()>=20,bestGenMatch->pt(),std::string("L1MuonPt20HoRecoAboveThrFilt"));
							histogramBuilder.fillEfficiency(bl1Muon->pt()>=25,bestGenMatch->pt(),std::string("L1MuonPt25HoRecoAboveThrFilt"));
						}

						//This one is filled for the sake of completeness. The SiPM regions are hardcoded in the class!!
						if (MuonHOAcceptance::inSiPMGeom(l1Muon_eta,l1Muon_phi/*,deltaR_Max,deltaR_Max*/)){
							histogramBuilder.fillCountHistogram(std::string("L1MuonAboveThrInAccNotDeadInSipm"));
						}
					}
				}
				histogramBuilder.fillEnergyHistograms(matchedRecHit->energy(),std::string("L1MuonWithHoMatchAboveThr"));
				histogramBuilder.fillEtaPhiHistograms(hoEta,hoPhi,std::string("L1MuonWithHoMatchAboveThr_HO"));
				histogramBuilder.fillDeltaEtaDeltaPhiHistograms(l1Muon_eta,hoEta,l1Muon_phi, hoPhi,std::string("L1MuonWithHoMatchAboveThr"));
				histogramBuilder.fillL1MuonPtHistograms(bl1Muon->pt(),std::string("L1MuonWithHoMatchAboveThr"));
				histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),std::string("L1MuonWithHoMatchAboveThr"));
				//Make the pseudo trig rate plot
				for (int i = 0; i < 200; i+=2) {
					if(bl1Muon->pt() >= i)
						histogramBuilder.fillTrigRateHistograms(i,l1MuonWithHoMatch_key);
				}
				const reco::GenParticle* bestGenMatch = getBestGenMatch(bl1Muon->eta(),bl1Muon->phi());
				if(bestGenMatch){
					//first argument is the condition for a muon trigger object to pass
					//Second is the pt of the "real" particle
					histogramBuilder.fillEfficiency(bl1Muon->pt()>=5,bestGenMatch->pt(),std::string("L1MuonPt5HoRecoAboveThr"));
					histogramBuilder.fillEfficiency(bl1Muon->pt()>=10,bestGenMatch->pt(),std::string("L1MuonPt10HoRecoAboveThr"));
					histogramBuilder.fillEfficiency(bl1Muon->pt()>=15,bestGenMatch->pt(),std::string("L1MuonPt15HoRecoAboveThr"));
					histogramBuilder.fillEfficiency(bl1Muon->pt()>=20,bestGenMatch->pt(),std::string("L1MuonPt20HoRecoAboveThr"));
					histogramBuilder.fillEfficiency(bl1Muon->pt()>=25,bestGenMatch->pt(),std::string("L1MuonPt25HoRecoAboveThr"));
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
				histogramBuilder.fillEtaPhiHistograms(bl1Muon->eta(), bl1Muon->phi(), std::string("L1MuonWithHoMatchAboveThr_L1Mu"));
			}// E > thr.
		}
	}// For loop over all l1muons
	histogramBuilder.fillMultiplicityHistogram(countGenMatches,std::string("nL1WithGenMatch"));
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
				histogramBuilder.fillEnergyHistograms(hoRecoIt->energy(),std::string("NoL1"));
				histogramBuilder.fillEtaPhiHistograms(ho_eta, ho_phi, std::string("NoL1"));
				histogramBuilder.fillEnergyVsPosition(ho_eta,ho_phi,hoRecoIt->energy(),std::string("NoL1"));
			}
		}
		histogramBuilder.fillMultiplicityHistogram(recHitAbThrNoL1Counter,std::string("NoL1"));
//		ofstream myfile;
//		myfile.open ("eventList_NoL1Muon.txt",std::ios::app);
//		myfile << iEvent.id().run() << "\t" << iEvent.id().luminosityBlock() << "\t" << iEvent.id().event() << std::endl;
//		myfile.close();
	} else{
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
			const HORecHit* matchedRecHit = HoMatcher::matchByEMaxDeltaR(genEta,genPhi,deltaR_Max,*hoRecoHits,*caloGeo);
			if(matchedRecHit){
				double hoEta = caloGeo->getPosition(matchedRecHit->id()).eta();
				double hoPhi = caloGeo->getPosition(matchedRecHit->id()).phi();
				histogramBuilder.fillDeltaEtaDeltaPhiEnergyHistogram(genEta,hoEta,genPhi,hoPhi,matchedRecHit->energy(),std::string("WithSingleMu"));
				if(matchedRecHit->energy() >= threshold){
					histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,std::string("WithSingleMuAboveThr"));
					if (MuonHOAcceptance::inGeomAccept(genEta,genPhi/*,deltaR_Max,deltaR_Max*/)){
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,std::string("WithSingleMuGeomAcc"));
						if (MuonHOAcceptance::inNotDeadGeom(genEta,genPhi/*,deltaR_Max,deltaR_Max*/)){
							histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,std::string("WithSingleMuNotDead"));
						}
					}
				}
			}//Matched rec hit
		}//Gen particle loop
	}

	if(!singleMu3Trig){
		histogramBuilder.fillCountHistogram(std::string("NoSingleMu"));
		histogramBuilder.fillMultiplicityHistogram(l1Muons->size(),std::string("NoSingleMu_L1Muon"));

		//################################
		//################################
		// Match Ho to gen info and try to recover mu trigger
		//################################
		//################################
		int matchFailCounter = 0;
		for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
				genIt != truthParticles->end(); genIt++){
			//Check for muons in Full barrel only
			//Try to find a corresponding Gen Muon
			float genEta = genIt->eta();
			float genPhi = genIt->phi();
			if(l1Muons->size()>0){
				histogramBuilder.fillCountHistogram(std::string("NoTriggerButL1Muons"));
			}
	//		const l1extra::L1MuonParticle* l1Ref = getBestL1MuonMatch(genEta,genPhi);
			//Inspect the cases where there was no matching to an L1Muon possible
//			if(!l1Ref){//FIXME: inspect also the case where there were l1 objects
			matchFailCounter++;
			histogramBuilder.fillPtHistogram(genIt->pt(),std::string("NoSingleMu"));
			const HORecHit* matchedRecHit = HoMatcher::matchByEMaxDeltaR(genEta,genPhi,deltaR_Max,*hoRecoHits,*caloGeo);
			//Check whether matching to a rec hit was possible
			if(matchedRecHit){
				double hoEta = caloGeo->getPosition(matchedRecHit->id()).eta();
				double hoPhi = caloGeo->getPosition(matchedRecHit->id()).phi();
				histogramBuilder.fillEtaPhiHistograms(hoEta,hoPhi,std::string("NoSingleMu_Ho"));
				histogramBuilder.fillDeltaEtaDeltaPhiEnergyHistogram(genEta,hoEta,genPhi,hoPhi,matchedRecHit->energy(),std::string("NoSingleMu"));
				histogramBuilder.fillPtHistogram(genIt->pt(),std::string("NoSingleMuButHo"));
				histogramBuilder.fillEfficiency(matchedRecHit->energy()>=threshold,genIt->pt(),"energyTurnOn");
				//Is the energy above threshold
				if(matchedRecHit->energy() >= threshold){
					histogramBuilder.fillPtHistogram(genIt->pt(),std::string("NoSingleMuAboveThr"));
					histogramBuilder.fillEtaPhiHistograms(hoEta,hoPhi,std::string("NoSingleMuAboveThr_Ho"));
					histogramBuilder.fillEnergyHistograms(matchedRecHit->energy(),std::string("NoSingleMu"));
					histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),std::string("NoSingleMu"));
					histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,std::string("NoSingleMu"));
					histogramBuilder.fillTimeHistogram(matchedRecHit->time(),std::string("NoSingleMu"));
					histogramBuilder.fillDeltaEtaDeltaPhiEnergyHistogram(genEta,hoEta,genPhi,hoPhi,matchedRecHit->energy(),std::string("NoSingleMuAboveThr"));
					histogramBuilder.fillEfficiency(matchedRecHit->energy()>=threshold,genIt->pt(),"energyTurnOnAboveThr");
					if (MuonHOAcceptance::inGeomAccept(genEta,genPhi/*,deltaR_Max,deltaR_Max*/)){
						histogramBuilder.fillCountHistogram(std::string("NoSingleMuAboveThrInAcc"));
						histogramBuilder.fillPtHistogram(genIt->pt(),std::string("NoSingleMuAboveThrInAcc"));
						histogramBuilder.fillEtaPhiHistograms(hoEta,hoPhi,std::string("NoSingleMuAboveThrInAcc_Ho"));
						if (MuonHOAcceptance::inNotDeadGeom(genEta,genPhi/*,deltaR_Max,deltaR_Max*/)){
							histogramBuilder.fillEtaPhiHistograms(hoEta,hoPhi,std::string("NoSingleMuAboveThrInAccNotDead_Ho"));
							histogramBuilder.fillPtHistogram(genIt->pt(),std::string("NoSingleMuAboveThrInAccNotDead"));
							histogramBuilder.fillEnergyHistograms(matchedRecHit->energy(),std::string("NoSingleMuFilt"));
							histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),std::string("NoSingleMuFilt"));
							histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,std::string("NoSingleMuFilt"));
							histogramBuilder.fillDeltaEtaDeltaPhiEnergyHistogram(genEta,hoEta,genPhi,hoPhi,matchedRecHit->energy(),std::string("NoSingleMuFilt"));
						}
					}
				}
			}//If matched rechit
			else{
				histogramBuilder.fillPtHistogram(genIt->pt(),std::string("NoSingleMuNoRecHit"));
				histogramBuilder.fillEtaPhiHistograms(genIt->eta(),genIt->phi(),std::string("NoSingleMuNoRecHit"));

			}
			//}//No Match to a l1 reference
			/**
			 * #############################################
			 * # Use TrackDetMatchInfo to see where the gen particle ends up in HO. When selecting only events, where gen is in
			 * # the geometric acceptance of HO, this gives a more realistic estimator for the number of recoverable
			 * # Triggers
			 * #############################################
			 */
			GlobalPoint vertexPoint(genIt->vertex().X(),genIt->vertex().Y(),genIt->vertex().Z());
			GlobalVector mom (genIt->momentum().x(),genIt->momentum().y(),genIt->momentum().z());
			int charge = genIt->charge();
			const FreeTrajectoryState *freetrajectorystate_ = new FreeTrajectoryState(vertexPoint, mom ,charge , &(*theMagField));
			TrackDetMatchInfo * muMatch = new TrackDetMatchInfo(assoc.associate(iEvent, iSetup, *freetrajectorystate_, assocParams));

			double muMatchPhi = muMatch->trkGlobPosAtHO.phi();
			double muMatchEta = muMatch->trkGlobPosAtHO.eta();

			histogramBuilder.fillEtaPhiHistograms(genEta,genPhi,std::string("NoTrgGenAny"));
			histogramBuilder.fillEtaPhiHistograms(muMatchEta,muMatchPhi,std::string("NoTrgTdmiAny"));
			//The muon needs to hit the HO geometric acceptance
			if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi)){
				histogramBuilder.fillCountHistogram("NoTrgTdmiInGA");
				histogramBuilder.fillEnergyVsPosition(muMatchEta,muMatchPhi,muMatch->hoCrossedEnergy(),std::string("NoTrgTdmiXedE"));
				const HORecHit* matchedRecHit = HoMatcher::matchByEMaxDeltaR(muMatchEta,muMatchPhi,deltaR_Max,*hoRecoHits,*caloGeo);
				//Where is the Rec hit in a delta R cone with the largest E?
				//Did we find any?
				if(matchedRecHit){
					double hoEta = caloGeo->getPosition(matchedRecHit->id()).eta();
					double hoPhi = caloGeo->getPosition(matchedRecHit->id()).phi();
					histogramBuilder.fillDeltaEtaDeltaPhiHistograms(muMatchEta,hoEta,muMatchPhi,hoPhi,std::string("NoTrgTdmi"));
					//Apply energy cut on the matched RecHit
					if(matchedRecHit->energy() >= threshold ){
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(muMatchEta,hoEta,muMatchPhi,hoPhi,std::string("NoTrgTdmiAboveThr"));
						histogramBuilder.fillDeltaEtaDeltaPhiHistograms(genEta,hoEta,genPhi,hoPhi,std::string("NoTrgGenAboveThr"));
						histogramBuilder.fillEnergyVsPosition(muMatchEta,muMatchPhi,muMatch->hoCrossedEnergy(),std::string("NoTrgTdmiAboveThrXedE"));
						histogramBuilder.fillEnergyVsPosition(hoEta,hoPhi,matchedRecHit->energy(),std::string("NoTrgTdmiAboveThrHoE"));
					//inspect the crossed energy, when the matched Rec hit in the cone was below threshold
					} else{
						histogramBuilder.fillEnergyVsPosition(muMatchEta,muMatchPhi,muMatch->hoCrossedEnergy(),std::string("NoTrgTdmiBelowThrXedE"));
					}
				//Count the events, where we could not match a Rec hit in the delta dR cone
				} else{
					histogramBuilder.fillCountHistogram(std::string("NoTrgHoMatchFail"));
					histogramBuilder.fillEtaPhiHistograms(muMatchEta,muMatchPhi,std::string("NoTrgHoMatchFail"));
				}
			}
		}//Loop over gen particles
		histogramBuilder.fillMultiplicityHistogram(matchFailCounter,std::string("NoSingleMu_MatchingFail"));
	}//Not single mu trg
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
		for(reco::GenParticleCollection::const_iterator genIt = truthParticles->begin();
				genIt != truthParticles->end(); genIt++){
			float genEta = genIt->eta();
			float genPhi = genIt->phi();

			//Create the Track det match info
			GlobalPoint vertexPoint(genIt->vertex().X(),genIt->vertex().Y(),genIt->vertex().Z());
			GlobalVector mom (genIt->momentum().x(),genIt->momentum().y(),genIt->momentum().z());
			int charge = genIt->charge();
			const FreeTrajectoryState *freetrajectorystate_ = new FreeTrajectoryState(vertexPoint, mom ,charge , &(*theMagField));
			TrackDetMatchInfo * muMatch = new TrackDetMatchInfo(assoc.associate(iEvent, iSetup, *freetrajectorystate_, assocParams));

			double muMatchPhi = muMatch->trkGlobPosAtHO.phi();
			double muMatchEta = muMatch->trkGlobPosAtHO.eta();

			//Require the muon to hit the HO area
			if(MuonHOAcceptance::inGeomAccept(muMatchEta,muMatchPhi)){

				histogramBuilder.fillCountHistogram("SMuTrgTdmiInGA");
				const l1extra::L1MuonParticle* l1Ref = getBestL1MuonMatch(genEta,genPhi);
				if(l1Ref){
					histogramBuilder.fillCountHistogram("SMuTrgFoundL1Match");
					float l1Muon_eta = l1Ref->eta();
					float l1Muon_phi = l1Ref->phi();
					//Fill the efficiency for gen muons that were matched to an l1 object
					//first argument is the condition for a muon trigger object to pass
					//Second is the pt of the "real" particle
					histogramBuilder.fillEfficiency(l1Ref->pt()>=5,genIt->pt(),std::string("SMuTrgL1AndGenMatch"));
					histogramBuilder.fillEfficiency(l1Ref->pt()>=10,genIt->pt(),std::string("SMuTrgL1AndGenMatch"));
					histogramBuilder.fillEfficiency(l1Ref->pt()>=15,genIt->pt(),std::string("SMuTrgL1AndGenMatch"));
					histogramBuilder.fillEfficiency(l1Ref->pt()>=20,genIt->pt(),std::string("SMuTrgL1AndGenMatch"));
					histogramBuilder.fillEfficiency(l1Ref->pt()>=25,genIt->pt(),std::string("SMuTrgL1AndGenMatch"));

					const HORecHit* matchedRecHit = HoMatcher::matchByEMaxDeltaR(l1Muon_eta,l1Muon_phi,deltaR_Max,*hoRecoHits,*caloGeo);
					//Check whether an HO match could be found by the delta R method
					if(matchedRecHit){
						histogramBuilder.fillCountHistogram("SMuTrgFoundHoMatch");
//						double hoEta,hoPhi;
//						hoEta = caloGeo->getPosition(matchedRecHit->detid()).eta();
//						hoPhi = caloGeo->getPosition(matchedRecHit->detid()).phi();
						//inspect the data where HO was above the threshold
						if(matchedRecHit->energy() >= threshold){
							histogramBuilder.fillCountHistogram("SMuTrgHoAboveThr");
							histogramBuilder.fillEfficiency(l1Ref->pt()>=5,genIt->pt(),std::string("SMuTrgL1AndGenMatchHoAboveThr"));
							histogramBuilder.fillEfficiency(l1Ref->pt()>=10,genIt->pt(),std::string("SMuTrgL1AndGenMatchHoAboveThr"));
							histogramBuilder.fillEfficiency(l1Ref->pt()>=15,genIt->pt(),std::string("SMuTrgL1AndGenMatchHoAboveThr"));
							histogramBuilder.fillEfficiency(l1Ref->pt()>=20,genIt->pt(),std::string("SMuTrgL1AndGenMatchHoAboveThr"));
							histogramBuilder.fillEfficiency(l1Ref->pt()>=25,genIt->pt(),std::string("SMuTrgL1AndGenMatchHoAboveThr"));
						}
					}
				}
			}
		}//Gen loop
	}
}

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
		cout << coutPrefix << "Error Code " << iErrorCode;
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
	cout << coutPrefix << "getL1GtRunCache" << endl;
	cout << coutPrefix << "UseL1EventSetup: " << useL1EventSetup << "UseL1GtTriggerMenuLite :"
			<< useL1GtTriggerMenuLite << endl;
	m_l1GtUtils.getL1GtRunCache(iRun, evSetup, useL1EventSetup, useL1GtTriggerMenuLite);

}


// ------------ method called when ending the processing of a run  ------------

void 
hoMuonAnalyzer::endRun(const edm::Run& iRun, const edm::EventSetup& evSetup)
{

	//Only interested in unique values
	listL1MuonPt.sort();
	listL1MuonPt.unique();  //NB it is called after sort
	cout << coutPrefix << "The list contains " << listL1MuonPt.size() << "unique entries:";
	std::list<float>::iterator it;
	for (it=listL1MuonPt.begin(); it!=listL1MuonPt.end(); ++it){
		cout << ' ' << *it;
	}
	cout << endl;
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
 * Helper Functions for Filter
 *############################
 */

const l1extra::L1MuonParticle* hoMuonAnalyzer::getMatchedL1Object(trigger::TriggerObject hltObject
		,edm::Handle<l1extra::L1MuonParticleCollection> l1muons){
	for(unsigned int i = 0; i < l1muons->size(); i++){
		const l1extra::L1MuonParticle* l1muon = &(l1muons->at(i));
		double hltPhi,hltEta;
		double l1Phi,l1Eta;
		hltEta = hltObject.eta();
		hltPhi = hltObject.phi();
		l1Eta = l1muon->eta();
		l1Phi = l1muon->phi();
		if(FilterPlugin::isInsideDeltaR(hltEta,l1Eta,hltPhi,l1Phi,deltaR_Max))
			return l1muon;
	}
	return NULL;
}

/**
 FilterPlugin::isInsideRCutue, if the trigger object has a delta R match to a l1muon object
 */
bool hoMuonAnalyzer::hasL1Match(trigger::TriggerObject hltObject,edm::Handle<l1extra::L1MuonParticleCollection> l1muons){
	for(unsigned int i = 0; i < l1muons->size(); i++){
		const l1extra::L1MuonParticle* l1muon = &(l1muons->at(i));
		double hltPhi,hltEta;
		double l1Phi,l1Eta;
		hltEta = hltObject.eta();
		hltPhi = hltObject.phi();
		l1Eta = l1muon->eta();
		l1Phi = l1muon->phi();
		if(FilterPlugin::isInsideDeltaR(hltEta,l1Eta,hltPhi,l1Phi,deltaR_Max))
			return true;
	}
	return false;
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
/**
 * Returns a pointer to the closest l1 muon particle of all particles that are closer
 * than delta R given by delta R max
 */
const l1extra::L1MuonParticle* hoMuonAnalyzer::getBestL1MuonMatch(float eta, float phi){
	const l1extra::L1MuonParticle* bestL1 = 0;
	float bestDR = 999.;
	l1extra::L1MuonParticleCollection::const_iterator l1It = l1Muons->begin();
	l1extra::L1MuonParticleCollection::const_iterator l1End = l1Muons->end();
	for(; l1It!=l1End; ++l1It) {
		if (abs(l1It->pdgId()) == 13 ) {
			float genPhi = l1It->phi();
			float genEta = l1It->eta();
			float dR = deltaR(eta,phi,genEta,genPhi);
			if (dR < deltaR_L1MuonMatching && dR < bestDR) { // CB get it from CFG
				bestDR = dR;
				bestL1 = &(*l1It);
			}
		}
	}
	return bestL1;
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

//define this as a plug-in
DEFINE_FWK_MODULE(hoMuonAnalyzer);
