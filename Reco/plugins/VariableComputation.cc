/* 
 * Computation of variables.
 */

/**
	@Author: Thorben Quast <tquast>
		22 November 2017
		thorben.quast@cern.ch / thorben.quast@rwth-aachen.de
*/


// system include files
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <map>
#include <math.h>
// user include files
#include "FWCore/Framework/interface/EDProducer.h"
#include "FWCore/Framework/interface/Frameworkfwd.h"
#include "FWCore/Framework/interface/Event.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "HGCal/CondObjects/interface/HGCalCondObjectTextIO.h"
#include "HGCal/CondObjects/interface/HGCalElectronicsMap.h"
#include "HGCal/DataFormats/interface/HGCalTBElectronicsId.h"
#include "HGCal/DataFormats/interface/HGCalTBRunData.h"	//for the runData type definition
#include "HGCal/DataFormats/interface/HGCalTBWireChamberData.h"
#include "HGCal/DataFormats/interface/HGCalTBRecHitCollections.h"
#include "HGCal/DataFormats/interface/HGCalTBClusterCollection.h"
#include "HGCal/DataFormats/interface/HGCalTBRecHit.h"
#include "HGCal/DataFormats/interface/HGCalTBCommonModeNoise.h"
#include "HGCal/DataFormats/interface/HGCalTBDWCTrack.h"
#include "HGCal/CondObjects/interface/HGCalTBDetectorLayout.h"

#include "HGCal/Geometry/interface/HGCalTBCellVertices.h"
#include "HGCal/Geometry/interface/HGCalTBTopology.h"
#include "HGCal/Geometry/interface/HGCalTBGeometryParameters.h"

#include "HGCal/Reco/interface/PositionResolutionHelpers.h"
#include "HGCal/Reco/interface/Sensors.h"

#include "TFile.h"
#include "TH2F.h"
#include "TH1F.h"  
#include <sstream>
#include <fstream>
#include <iomanip>
#include <set>

double X0PosSeptember2017[18] = {2.764, 4.385, 6.005, 7.625, 9.245, 12.837, 16.267, 23.636, 26.431, 29.226, 32.215 ,35.01, 37.805, 41.739, 44.728, 47.717, 50.706, 55.846};
double Lambda0PosSeptember2017[18] = {0.168, 0.245, 0.323, 0.401, 0.478, 0.649, 0.853, 1.351, 1.587, 1.823, 2.059, 2.295, 2.531, 2.886, 3.122, 3.358, 3.594, 3.994};
double weightsSeptember2017[18] = {24.523, 17.461, 17.461, 17.461, 27.285, 38.737, 75.867, 83.382, 55.394, 55.823, 55.823, 55.394, 66.824, 67.253, 56.252, 56.252, 79.871, 103.49};
double MIP2GeVSeptember2017 = 84.9e-6;

std::vector<double> getEigenValuesOfSymmetrix3x3(double A11, double A22, double A33, double A12, double A13, double A23) {
	//eigenvalue computation: https://arxiv.org/pdf/1306.6291.pdf
	double b = A11 + A22 + A33;
	double c = A11*A22 + A11*A33 + A22*A33 - pow(A12,2) - pow(A13,2) - pow(A23,2);
	double d = A11*pow(A23,2) + A22*pow(A13,2) + A33*pow(A12,2) - A11*A22*A33 - 2*A12*A13*A23;

	double p = pow(b, 2) - 3*c;
	double q = 2*pow(b, 3) - 9*b*c - 27*d;

	double delta = acos(q/sqrt(4*pow(p, 3)));
	double lambda1 = (b+2*sqrt(p)*cos(delta/3))/3;
	double lambda2 = (b+2*sqrt(p)*cos((delta+2*M_PI)/3))/3;
	double lambda3 = (b+2*sqrt(p)*cos((delta-2*M_PI)/3))/3;

	std::vector<double> EVs;
	EVs.push_back(lambda1);
	EVs.push_back(lambda2);
	EVs.push_back(lambda3);
	std::sort(EVs.begin(), EVs.end());

	return EVs;
}

//#define DEBUG

class VariableComputation : public edm::EDProducer {
	public:
		explicit VariableComputation(const edm::ParameterSet&);
		~VariableComputation();
		static void fillDescriptions(edm::ConfigurationDescriptions& descriptions);

	private:
		virtual void beginJob();
		virtual void produce(edm::Event& , const edm::EventSetup&);
		virtual void endJob();


		// ----------member data ---------------------------
		edm::EDGetTokenT<HGCalTBRecHitCollection> HGCalTBRecHitCollection_Token;	 		
		edm::EDGetTokenT<RunData> RunDataToken;	
		edm::EDGetTokenT<WireChambers> DWCToken;		
		edm::EDGetTokenT<HGCalTBDWCTrack> DWCTrackToken;		

		std::string m_UserRecordCollectionName;

		std::string m_electronicMap;
		std::string m_detectorLayoutFile;
		struct {
			HGCalElectronicsMap emap_;
			HGCalTBDetectorLayout layout_;
		} essource_;

		std::string m_layerPositionFile;

		int m_NHexaBoards;
		int m_NLayers;


		std::map<int, double> layerPositions;

			
		//energy sums:
		double MIP_cut_for_energy;	
		std::map<int, SensorHitMap*> Sensors;

		double energyAll_tot, energyE1_tot, energyE7_tot, energyE19_tot, energyE37_tot, energyE61_tot;
		double energyAll_weight, energyE1_weight, energyE7_weight, energyE19_weight, energyE37_weight, energyE61_weight;
		std::vector<double> energyAll_layer, energyE1_layer, energyE7_layer, energyE19_layer, energyE37_layer, energyE61_layer;
		std::vector<int> NAll_layer, NE1_layer, NE7_layer, NE19_layer, NE37_layer, NE61_layer;

		double depthX0, depthLambda0;

		//distance information
		std::vector<double> mainCoreWidth;
		std::vector<double> cellDistance_layer;

		double Ixx, Iyy, Izz, Ixy, Ixz, Iyz;
};

VariableComputation::VariableComputation(const edm::ParameterSet& iConfig) {	
	
	HGCalTBRecHitCollection_Token = consumes<HGCalTBRecHitCollection>(iConfig.getParameter<edm::InputTag>("HGCALTBRECHITS"));
	RunDataToken= consumes<RunData>(iConfig.getParameter<edm::InputTag>("RUNDATA"));
	DWCToken= consumes<WireChambers>(iConfig.getParameter<edm::InputTag>("MWCHAMBERS"));
	DWCTrackToken= consumes<HGCalTBDWCTrack>(iConfig.getParameter<edm::InputTag>("DWCTRACKS"));
	
	m_UserRecordCollectionName = iConfig.getUntrackedParameter<std::string>("UserRecordCollectionName","DoubleUserRecords");
	
	m_electronicMap = iConfig.getUntrackedParameter<std::string>("ElectronicMap","HGCal/CondObjects/data/map_CERN_Hexaboard_28Layers_AllFlipped.txt");
	m_detectorLayoutFile = iConfig.getUntrackedParameter<std::string>("DetectorLayout","HGCal/CondObjects/data/layerGeom_oct2017_h2_17layers.txt");
	m_layerPositionFile = iConfig.getParameter<std::string>("layerPositionFile");
	m_NHexaBoards= iConfig.getUntrackedParameter<int>("NHexaBoards", 17);
	m_NLayers= iConfig.getUntrackedParameter<int>("NLayers", 17);

	produces <UserRecords<double> >(m_UserRecordCollectionName);


	MIP_cut_for_energy = 4.;
}

VariableComputation::~VariableComputation() {
	return;
}

// ------------ method called for each event  ------------
void VariableComputation::produce(edm::Event& event, const edm::EventSetup& setup) {
	edm::Handle<RunData> rd;
	event.getByToken(RunDataToken, rd);
	

	edm::Handle<HGCalTBDWCTrack> dwctrack;
	event.getByToken(DWCTrackToken, dwctrack);

	edm::Handle<WireChambers> dwcs;
	event.getByToken(DWCToken, dwcs);

	edm::Handle<HGCalTBRecHitCollection> Rechits;
	event.getByToken(HGCalTBRecHitCollection_Token, Rechits);

	std::auto_ptr<UserRecords<double> > UR(new UserRecords<double>);
	
	/**********                                 ****************/
	

	//first some basic information
	UR->add("eventID", rd->event);
	UR->add("run", rd->run);
	UR->add("pdgID", rd->pdgID);
	UR->add("beamEnergy", rd->energy);
	UR->add("configuration", rd->configuration);
	UR->add("runType", rd->runType);
	
	//



	/**********                                 ****************/
	//filling and sorting of the rechits:

	std::vector<double> rechit_energies;
	std::vector<HGCalTBRecHit> rechits;
	std::vector<HGCalTBRecHit> MIPhits;
	std::vector<HGCalTBRecHit> noisehits;

	double M=0, xmean=0, ymean=0, zmean=0;

	for(auto Rechit : *Rechits) {	
		int layer = (Rechit.id()).layer();
		if ( Sensors.find(layer) == Sensors.end() ) {
			Sensors[layer] = new SensorHitMap(layer);
			Sensors[layer]->setSensorSize(133);
		}

		if (Rechit.energy() > MIP_cut_for_energy) {
			Sensors[layer]->addHit(Rechit, 1.);
			if ((Rechit.id()).cellType() == 0) {
				double x = Rechit.getCellCenterCartesianCoordinate(0);
				double y = Rechit.getCellCenterCartesianCoordinate(1);
				double z = layerPositions[layer];
				//std::cout<<"Layer: "<<layer<<" z: "<<z<<std::endl;
				double m = Rechit.energy();
				M += m;
				xmean += m*x;
				ymean += m*y;
				zmean += m*z;
				
				rechit_energies.push_back(m);
				rechits.push_back(Rechit);
			}
		} else if (Rechit.energy() > 0.5) {
			if ((Rechit.id()).cellType() == 0) MIPhits.push_back(Rechit);
		} else  {
			if ((Rechit.id()).cellType() == 0) noisehits.push_back(Rechit);
		}

	}


	/**********                                 ****************/
	//mean positions

	xmean /= M;
	ymean /= M;
	zmean /= M;

	UR->add("xmean", xmean);
	UR->add("ymean", ymean);
	UR->add("zmean", zmean);


	/**********                                 ****************/
	//rechit spectra positions

	int NRechits = rechit_energies.size();
	int N25PercentsRechits = NRechits*1./4.;
	int N50PercentsRechits = NRechits*2./4.;
	int N75PercentsRechits = NRechits*3./4.;
	std::sort(rechit_energies.begin(), rechit_energies.end());

	UR->add("NRechits", NRechits);
	UR->add("NMIPHits", MIPhits.size());
	UR->add("NNoisehits", noisehits.size());
	UR->add("25PercentQuantileRechitSpectrum", (NRechits>4) ? rechit_energies[N25PercentsRechits-1] : -1.);
	UR->add("50PercentQuantileRechitSpectrum", (NRechits>4) ? rechit_energies[N50PercentsRechits-1] : -1.);
	UR->add("75PercentQuantileRechitSpectrum", (NRechits>4) ? rechit_energies[N75PercentsRechits-1] : -1.);


	/**********                                 ****************/
	//determine inertia tensor:
	Ixx=Iyy=Izz=Ixy=Ixz=Iyz=0;
	for(auto Rechit : *Rechits) {	
		int layer = (Rechit.id()).layer();
		if (Rechit.energy() > MIP_cut_for_energy) {
			if ((Rechit.id()).cellType() == 0) {
				double x = Rechit.getCellCenterCartesianCoordinate(0);
				double y = Rechit.getCellCenterCartesianCoordinate(1);
				double z = layerPositions[layer];
				//std::cout<<"Layer: "<<layer<<" z: "<<z<<std::endl;
				double m = Rechit.energy();

				Ixx += m*(pow(y-ymean, 2)+pow(z-zmean, 2));
				Iyy += m*(pow(x-xmean, 2)+pow(z-zmean, 2));
				Izz += m*(pow(x-xmean, 2)+pow(y-ymean, 2));
				Ixy -= m*(x-xmean)*(y-ymean);
				Ixz -= m*(x-xmean)*(z-zmean);
				Iyz -= m*(y-ymean)*(z-zmean);

			}
		}
	}

	Ixx /= M;
	Iyy /= M;
	Izz /= M;
	Ixy /= M;
	Ixz /= M;
	Iyz /= M;
	std::vector<double> I_EV = getEigenValuesOfSymmetrix3x3(Ixx, Iyy, Izz, Ixy, Ixz, Iyz);

	UR->add("Ixx", Ixx);
	UR->add("Iyy", Iyy);
	UR->add("Izz", Izz);
	UR->add("Ixy", Ixy);
	UR->add("Ixz", Ixz);
	UR->add("Iyz", Iyz);
	UR->add("I_EV1", I_EV[0]);
	UR->add("I_EV2", I_EV[1]);
	UR->add("I_EV3", I_EV[2]);


	/**********                                 ****************/
	
	//Energy information
	energyE1_tot = energyE7_tot = energyE19_tot = energyE37_tot = energyE61_tot = energyAll_tot = 0.;
	energyE1_weight = energyE7_weight = energyE19_weight = energyE37_weight = energyE61_weight = energyAll_weight = 0.;
	
	depthX0 = 0, depthLambda0 = 0;

	std::vector<std::pair<double, double> > relevantHitPositions;
	for (std::map<int, SensorHitMap*>::iterator it=Sensors.begin(); it!=Sensors.end(); it++) {
		//most intensive cell
		it->second->calculateCenterPosition(CONSIDERALL, MOSTINTENSIVE);
		energyE1_tot += it->second->getTotalWeight();
		energyE1_layer[it->first-1] = it->second->getTotalWeight();
		relevantHitPositions = it->second->getHitPositionsForPositioning();
		NE1_layer[it->first-1] = (int)relevantHitPositions.size();
		relevantHitPositions.clear();

		//one ring around
		it->second->calculateCenterPosition(CONSIDERSEVEN, LINEARWEIGHTING);
		energyE7_tot += it->second->getTotalWeight();
		energyE7_layer[it->first-1] = it->second->getTotalWeight();
		relevantHitPositions = it->second->getHitPositionsForPositioning();
		NE7_layer[it->first-1] = (int)relevantHitPositions.size();
		relevantHitPositions.clear();

		//two rings around
		it->second->calculateCenterPosition(CONSIDERNINETEEN, LINEARWEIGHTING);
		energyE19_tot += it->second->getTotalWeight();
		energyE19_layer[it->first-1] = it->second->getTotalWeight();
		relevantHitPositions = it->second->getHitPositionsForPositioning();
		NE19_layer[it->first-1] = (int)relevantHitPositions.size();
		relevantHitPositions.clear();
	
		//three rings around
 		it->second->calculateCenterPosition(CONSIDERTHIRTYSEVEN, LINEARWEIGHTING);
		energyE37_tot += it->second->getTotalWeight();
		energyE37_layer[it->first-1] = it->second->getTotalWeight();
		relevantHitPositions = it->second->getHitPositionsForPositioning();
		NE37_layer[it->first-1] = (int)relevantHitPositions.size();
		relevantHitPositions.clear();

     //four rings around
 		it->second->calculateCenterPosition(CONSIDERSIXTYONE, LINEARWEIGHTING);
		energyE61_tot += it->second->getTotalWeight();
		energyE61_layer[it->first-1] = it->second->getTotalWeight();
		relevantHitPositions = it->second->getHitPositionsForPositioning();
		NE61_layer[it->first-1] = (int)relevantHitPositions.size();
		relevantHitPositions.clear();
		

		//sum of all
		it->second->calculateCenterPosition(CONSIDERALL, LINEARWEIGHTING);
		energyAll_tot += it->second->getTotalWeight();
		energyAll_layer[it->first-1] = it->second->getTotalWeight();
		relevantHitPositions = it->second->getHitPositionsForPositioning();
		NAll_layer[it->first-1] = (int)relevantHitPositions.size();
		relevantHitPositions.clear();
	
		UR->add("E1PerE7_layer"+std::to_string(it->first), energyE1_layer[it->first-1]/energyE7_layer[it->first-1]);
		UR->add("E7PerE19_layer"+std::to_string(it->first), energyE7_layer[it->first-1]/energyE19_layer[it->first-1]);
		UR->add("E19PerE37_layer"+std::to_string(it->first), energyE19_layer[it->first-1]/energyE37_layer[it->first-1]);
		UR->add("E37PerE61_layer"+std::to_string(it->first), energyE37_layer[it->first-1]/energyE61_layer[it->first-1]);

		double weight = 0., MIP2GeV=1., X0 = 0., lambda0 = 0.;
		if (rd->configuration==2) {
			weight = weightsSeptember2017[it->first-1]*1e-3;
			MIP2GeV = MIP2GeVSeptember2017;
			X0 = X0PosSeptember2017[it->first-1];
			lambda0 = Lambda0PosSeptember2017[it->first-1];
		}

		depthX0 += X0*energyAll_layer[it->first-1];
		depthLambda0 += lambda0*energyAll_layer[it->first-1];

		energyE1_weight += energyE1_layer[it->first-1]*(MIP2GeV+weight); 
		energyE7_weight += energyE7_layer[it->first-1]*(MIP2GeV+weight); 
		energyE19_weight += energyE19_layer[it->first-1]*(MIP2GeV+weight); 
		energyE37_weight += energyE37_layer[it->first-1]*(MIP2GeV+weight); 
		energyE61_weight += energyE61_layer[it->first-1]*(MIP2GeV+weight); 
		energyAll_weight += energyAll_layer[it->first-1]*(MIP2GeV+weight); 


		//position resolution
		if (dwctrack->valid&&(dwctrack->referenceType==15) && (dwctrack->chi2_x<=5.) && (dwctrack->chi2_y<=5.)) { 
			//std::cout<<"Layer: "<<it->first<<std::endl;
			//std::cout<<"DWC X: "<<dwctrack->DWCExtrapolation_XY(it->first).first<<"  reco X: "<<it->second->getLabHitPosition().first<<std::endl;
			//std::cout<<"DWC Y: "<<dwctrack->DWCExtrapolation_XY(it->first).second<<"  reco Y: "<<it->second->getLabHitPosition().second<<std::endl;
			it->second->calculateCenterPosition(CONSIDERNINETEEN, LOGWEIGHTING_35_10);
			//investigate here
			//x = -x in DWC coordinate system
			UR->add("PosResX_layer"+std::to_string(it->first),(it->second->getLabHitPosition().first-dwctrack->DWCExtrapolation_XY(it->first).first));
			UR->add("PosResY_layer"+std::to_string(it->first),(it->second->getLabHitPosition().second-dwctrack->DWCExtrapolation_XY(it->first).second));
		}
	}
	//std::cout<<std::endl;

	depthX0 /= energyAll_tot;
	depthLambda0 /= energyAll_tot;

	UR->add("E1_tot", energyE1_tot);
	UR->add("E7_tot", energyE7_tot);
	UR->add("E19_tot", energyE19_tot);
	UR->add("E37_tot", energyE37_tot);
	UR->add("E61_tot", energyE61_tot);
	UR->add("EAll_tot", energyAll_tot);

	UR->add("E1_weight", energyE1_weight);
	UR->add("E7_weight", energyE7_weight);
	UR->add("E19_weight", energyE19_weight);
	UR->add("E37_weight", energyE37_weight);
	UR->add("E61_weight", energyE61_weight);
	UR->add("EAll_weight", energyAll_weight);

	UR->add("depthX0", depthX0);
	UR->add("depthLambda0", depthLambda0);

	double E_EE = 0, E_FH = 0;
	int last_layer_EE = 2; int last_layer_FH = 6;
	if (rd->configuration==2) {
		last_layer_EE=7;
		last_layer_FH=17;
	} //todo: October setup

	for (int l=0; l<last_layer_EE; l++) E_EE += energyAll_layer[l];
	for (int l=last_layer_EE; l<last_layer_FH; l++) E_FH += energyAll_layer[l];

	UR->add("E_EE", E_EE);
	UR->add("E_FH", E_FH);
	UR->add("E_EEperE_tot", E_EE/(E_EE+E_FH));
	UR->add("E_FHperE_tot", E_FH/(E_EE+E_FH));

	

	/**********                                 ****************/
	
	//Spacing information

	for (std::map<int, SensorHitMap*>::iterator it=Sensors.begin(); it!=Sensors.end(); it++) {
		//two rings around
		it->second->calculateCenterPosition(CONSIDERNINETEEN, LINEARWEIGHTING);		
		std::vector<double> gaussianFitParameters = it->second->fit2DGaussian();
		if (gaussianFitParameters[0] != 0) mainCoreWidth[it->first-1] = -1.;
		else mainCoreWidth[it->first-1] = sqrt(pow(gaussianFitParameters[3], 2) + pow(gaussianFitParameters[5],2));
		UR->add("width_E19_layer"+std::to_string(it->first), mainCoreWidth[it->first-1]);

		//consideration of all
		it->second->calculateCenterPosition(CONSIDERALL, LINEARWEIGHTING);	
		cellDistance_layer[it->first-1] = it->second->getDistanceBetweenMostIntenseCells();		

		UR->add("d2_maxE_layer"+std::to_string(it->first), cellDistance_layer[it->first-1]);
	}

	/**********                                 ****************/
		

	event.put(UR, m_UserRecordCollectionName);



	for (std::map<int, SensorHitMap*>::iterator it=Sensors.begin(); it!=Sensors.end(); it++) {
		delete (*it).second;
	};	Sensors.clear();	

}// analyze ends here

void VariableComputation::beginJob() {	
	for( int ilayer=0; ilayer<m_NLayers; ilayer++ ){
		energyAll_layer.push_back(0.);
		energyE1_layer.push_back(0.);
		energyE7_layer.push_back(0.);
		energyE19_layer.push_back(0.);
		energyE37_layer.push_back(0.);
		energyE61_layer.push_back(0.);
		NAll_layer.push_back(0);
		NE1_layer.push_back(0);
		NE7_layer.push_back(0);
		NE19_layer.push_back(0);
		NE37_layer.push_back(0);
		NE61_layer.push_back(0);
		mainCoreWidth.push_back(0.);
		cellDistance_layer.push_back(0.);
	}


	std::fstream file; 
	char fragment[100];
	int readCounter = -1;

	file.open(m_layerPositionFile.c_str(), std::fstream::in);

	std::cout<<"Reading file "<<m_layerPositionFile<<" -open: "<<file.is_open()<<std::endl;
	int layer=0;
	while (file.is_open() && !file.eof()) {
		readCounter++;
		file >> fragment;
		if (readCounter==0) layer=atoi(fragment);
		if (readCounter==1) {
			layerPositions[layer]=atof(fragment);
			readCounter=-1;
		}
	}

	
}

void VariableComputation::endJob() {
}

void VariableComputation::fillDescriptions(edm::ConfigurationDescriptions& descriptions) {
	edm::ParameterSetDescription desc;
	desc.setUnknown();
	descriptions.addDefault(desc);
}

//define this as a plug-in
DEFINE_FWK_MODULE(VariableComputation);