
//
//
// Package:    RecoBTag/PerformanceMeasurements
// Class:      OperatingPoints
//
/**\class PerformanceMeasurements/OperatingPoints

 Description:

 Author: Francisco Yumiceva
*/
//
// $Id: OperatingPoints.cc,v 1.10 2010/03/31 23:29:36 jindal Exp $
//
//

// header
#include "RecoBTag/PerformanceMeasurements/interface/OperatingPoints.h"

// CMSSW
#include "FWCore/ParameterSet/interface/ParameterSet.h"
#include "FWCore/MessageLogger/interface/MessageLogger.h"
#include "FWCore/Framework/interface/MakerMacros.h"
#include "FWCore/ServiceRegistry/interface/Service.h"
#include "CommonTools/UtilAlgos/interface/TFileService.h"

#include "DataFormats/Math/interface/deltaR.h"
#include "DataFormats/JetReco/interface/CaloJetCollection.h"
#include "DataFormats/JetReco/interface/CaloJet.h"
#include "DataFormats/JetReco/interface/Jet.h"
#include "DataFormats/BTauReco/interface/JetTag.h"
#include "JetMETCorrections/Objects/interface/JetCorrector.h"
// ROOT
#include "TGraphErrors.h"

using namespace edm;
using namespace reco;
using namespace std;

//
// constructors and destructors
//
OperatingPoints::OperatingPoints(const ParameterSet& iConfig)
{

    CaloJetCollectionTags_ = iConfig.getParameter<std::string>("Jets");

    // jet cuts
    MinJetPt_ = iConfig.getParameter<edm::ParameterSet>("jetcuts").getParameter<double>("MinPt");
    MaxJetEta_ = iConfig.getParameter<edm::ParameterSet>("jetcuts").getParameter<double>("MaxEta");
    MinNtrksInJet_ = iConfig.getParameter<edm::ParameterSet>("jetcuts").getParameter<int>("MinNtracks");
    MinTrkPtInJet_ = iConfig.getParameter<edm::ParameterSet>("jetcuts").getParameter<double>("MinTrkPt");
    MinNjets_ = iConfig.getParameter<edm::ParameterSet>("jetcuts").getParameter<int>("MinNjets");

    useJetCorr_ = iConfig.getParameter<bool>("ApplyJetCorrections");
    jetCorrLabel_ =  iConfig.getParameter<std::string>("jetCorrectionsLabel");
    std::cout<<" Use JetCorrections with Label "<<jetCorrLabel_ <<std::endl;

    // Flavour identification
    flavourSourcef = iConfig.getParameter<edm::InputTag>("flavourSource");

    bTagTrackEventIPTagInfos_ = iConfig.getParameter<std::string>("bTagTrackEventIPtagInfos");

    //
    // get operating points

    std::vector<edm::ParameterSet> config = iConfig.getUntrackedParameter<std::vector<edm::ParameterSet > >("OperatingPointsList");

    // type of operating points
    OPbyMistagRate_ = iConfig.getParameter<bool>("OperatingPointsbyMistagRate");

    for (std::vector<edm::ParameterSet>::const_iterator it = config.begin(); it != config.end() ; ++it)
    {

        std::string alias = (*it).getUntrackedParameter<std::string> ("alias");
        std::vector<edm::ParameterSet> avec = (*it).getUntrackedParameter<std::vector<edm::ParameterSet> >("OperatingPoints");
        double min = (*it).getUntrackedParameter<double> ("MinimumDiscriminator");
        double max = (*it).getUntrackedParameter<double> ("MaximumDiscriminator");

        std::map< std::string, double > mapOP;
        for (std::vector<edm::ParameterSet>::const_iterator imapOP = avec.begin(); imapOP != avec.end(); ++imapOP)
        {

            mapOP[(*imapOP).getUntrackedParameter<std::string> ("name")] =
                (*imapOP).getUntrackedParameter<double> ("cut");
        }

        WorkingPoint tmpwp((*it).getUntrackedParameter<edm::InputTag> ("collection"),
                           alias,
                           min,
                           max,
                           mapOP );

        wp.push_back(tmpwp);
        //(wp.end()-1)->print();

        wp_map[alias] = tmpwp;


        TaggerPerformances_[alias].Set(alias);
        TaggerPerformances_[alias].SetMinDiscriminator(min);
        TaggerPerformances_[alias].SetMaxDiscriminator(max);
    }

    // open output file to store results
    outputFile_  = iConfig.getUntrackedParameter<std::string>("outputFile");

    // create or update output root file
    rootFile_ = TFile::Open(outputFile_.c_str(),"RECREATE");

    // verbose std output
    debug_ = iConfig.getUntrackedParameter<bool>("debug", false);
}

OperatingPoints::~OperatingPoints()
{

    rootFile_->cd();

    std::vector< TGraph* > gVector;

    if ( OPbyMistagRate_ )
    {

        cout << "\n b-tagging Operating Points estimated by udsg-mistagging rate " << endl;
        cout << "==============================================================\n" << endl;


        //cout << "\\begin{table}[btp]" << endl;
        //cout << "\\begin{center}" << endl;
        cout << "\\begin{tabular}{|c|c|c|c|} \\hline" << endl;


    }

    for (std::map<std::string, S8bPerformance>::const_iterator iperf = TaggerPerformances_.begin(); iperf!= TaggerPerformances_.end(); ++iperf )
    {

        S8bPerformance Perf = iperf->second;

        Perf.Eval();


        TGraphErrors *gTb = Perf.EfficiencyGraph("b");
        TGraphErrors *gTc = Perf.EfficiencyGraph("c");
        TGraphErrors *gTl = Perf.EfficiencyGraph("udsg");
        gVector.push_back( gTb );
        gVector.push_back( gTc );
        gVector.push_back( gTl );

        TGraph *discTl = Perf.DiscriminatorGraph("udsg");
        discTl->Sort();
        gVector.push_back( discTl );

        if ( OPbyMistagRate_ )
        {
            // reverse axis of graph
            TGraphErrors *g_reverse = new TGraphErrors(gTl->GetN(),gTl->GetY(),gTl->GetX(),gTl->GetEY(),gTl->GetEX());
            g_reverse->Sort();


            // get operating point cuts
            WorkingPoint apt = wp_map[iperf->first];

            cout << "\\multicolumn{1}{|c|}{ " << apt.inputTag().label() << " ( " << apt.alias() << " ) }\\hline" << endl;

            //cout << " Tagger Name: " << apt.inputTag().label() << ", Alias: " << apt.alias() << endl;

            std::map<std::string, double > list_cuts = apt.list();

            double *xarr = new double[ (int)list_cuts.size() ];
            double *yarr = new double[ (int)list_cuts.size() ];
            int ii = 0;

            cout << " OP Alias   &   udsg-mistagging   &   b-efficiency   &   discriminator cut \\hline" << endl;

            for (std::map<std::string, double >::const_iterator icut = list_cuts.begin(); icut != list_cuts.end(); ++icut)
            {

                double disc_cut = discTl->Eval( icut->second );
                double b_eff = g_reverse->Eval( icut->second );

                // alias, udsg-mistagging, b-efficiency
                cout <<  icut->first << " & "
                     << setprecision(3) << icut->second << " & "
                     << setprecision(3) << b_eff << " & "
                     << setprecision(3) << disc_cut << " \\" << endl;

                //cout << icut->first << " : udsg-mistagging = " << setprecision(3) << icut->second << ", b-efficiency = " << setprecision(3) << b_eff << ", discriminator cut = " << setprecision(3) << disc_cut << endl;
                xarr[ii] = icut->second;
                yarr[ii] = b_eff;
                ii++;
            }
            cout << endl;

            TGraph *g_results = new TGraph( (int)list_cuts.size(), xarr, yarr );
            g_results->SetTitle( TString(apt.alias() + "_OP") );
            g_results->SetName( TString(apt.alias() + "_OP") );
            g_results->GetXaxis()->SetTitle("udsg-mistagging");
            g_results->GetYaxis()->SetTitle("b-efficiency");
            gVector.push_back( g_results );

            delete g_reverse;
            delete xarr;
            delete yarr;
        }

        cout << "\\hline" << endl;
        cout << "\\end{tabular}" << endl;

    }
    for (std::vector< TGraph* >::const_iterator iv = gVector.begin(); iv != gVector.end(); ++iv )
    {
        (*iv)->Write();
    }
    rootFile_->Close();
}


//
// member functions
//
void OperatingPoints::beginJob(edm::EventSetup const& iSetup)
{
    std::cout << "OperatingPoints: begin Job" << std::endl;
}

void OperatingPoints::endJob() {}

int OperatingPoints::TaggedJet(reco::CaloJet calojet,edm::Handle<reco::JetTagCollection > jetTags )
{
    double small = 1.e-5;
    int result = -1; // no tagged
    //int ith = -1;

    //std::cout << "calo jet: pz = " << calojet.pz() << " pt = " << calojet.pt() << std::endl;
    //for (size_t k=0; k<jetTags_testManyByType.size(); k++) {
    //  edm::Handle<std::vector<reco::JetTag> > jetTags = jetTags_testManyByType[k];

    //get label and module names


    //    std::cout <<" ECCO " << jetTags.product()<< std::endl;

    for (size_t t = 0; t < jetTags->size(); ++t)
    {
        edm::RefToBase<reco::Jet> jet_p = (*jetTags)[t].first;
        if (jet_p.isNull())
        {
            //std::cout << "-----------> JetTag::jet() returned null reference" << std::endl;
            continue;
        }
        //std::cout << "[TaggedJet]  calojet pt = " << calojet.pt() << " tagged jet pt = " << jet_p->pt() << std::endl;
        if (DeltaR<reco::Candidate>()( calojet, *jet_p ) < small)
        {

            result = (int) t;

        }
    }

    return result;
}


reco::JetFlavour OperatingPoints::getMatchedParton(const reco::CaloJet &jet)
{
    reco::JetFlavour jetFlavour;

    for ( JetFlavourMatchingCollection::const_iterator j = theJetPartonMapf->begin();
            j != theJetPartonMapf->end();
            j ++ )
    {
        RefToBase<Jet> aJet  = (*j).first;
        //      const JetFlavour aFlav = (*j).second;
        if ( (aJet->phi() - jet.phi())*(aJet->phi() - jet.phi())+
                (aJet->eta() - jet.eta())*(aJet->eta() - jet.eta()) < 1.e-5 )
        {
            // matched
            jetFlavour = reco::JetFlavour (aJet->p4(), math::XYZPoint(0,0,0), (*j).second.getFlavour());
        }
    }

    return jetFlavour;
}


// ------------ method called to produce the data  ------------
void OperatingPoints::analyze(const Event& iEvent, const EventSetup& iSetup)
{

    iEvent.getByLabel (flavourSourcef, theJetPartonMapf);

    // Calo Jets
    Handle< View<reco::CaloJet> > jetsColl;
    iEvent.getByLabel(CaloJetCollectionTags_, jetsColl);

    // Get the bTagTrackEventIPTagInfo collection
    Handle<std::vector<reco::TrackIPTagInfo> > bTagTrackEventIPTagInfos;
    iEvent.getByLabel(bTagTrackEventIPTagInfos_, bTagTrackEventIPTagInfos);

    //Handle<std::vector<reco::TrackIPTagInfo> > tagInfo;
    //iEvent.getByLabel("impactParameterTagInfos", tagInfo);


    // initialize jet corrector
    const JetCorrector *acorrector = 0;
    if (useJetCorr_ == true)
    {
        acorrector = JetCorrector::getJetCorrector(jetCorrLabel_,iSetup);
    }

    const View< reco::CaloJet > &theJets = *jetsColl;
    if (debug_) std::cout << "got jet collection" << std::endl;

    int jetIndex = 0;

    for (edm::View<reco::CaloJet>::const_iterator jet = theJets.begin(); jet!=theJets.end(); ++jet)
    {

        // get jet corrections
        double jetcorrection = 1.;
        if (useJetCorr_ == true)
        {
            jetcorrection =  acorrector->correction(*jet);
        }

        // Jet quality cuts part 1
        if ( (jet->pt() * jetcorrection ) <= MinJetPt_ || std::abs( jet->eta() ) >= MaxJetEta_ )
        {
            jetIndex++;
            continue;
        }

        // Get a vector of reference to the selected tracks in each jet
        TrackRefVector tracks( (*bTagTrackEventIPTagInfos)[jetIndex].selectedTracks() );

        for ( size_t index=0; index < tracks.size(); index++ )
        {
            TrackRef track = tracks[index];
            if (debug_)
            {
                std::cout << " track: pt= "<<track->pt() << " eta=" << track->eta() << std::endl;
            }
        }


        if (debug_) std::cout << " jet correction = " << jetcorrection << std::endl;

        if (debug_) std::cout << "jet pt=" << jet->pt() << " eta= " << jet->eta() << " phi= " << jet->phi() << std::endl;

        // jet flavor
        int JetFlavor = abs(getMatchedParton(*jet).getFlavour());
        if (debug_) std::cout << " jet flavor = " << JetFlavor << std::endl;

        std::map<std::string, bool> mymap;
        int ith_tagged = -1;

        for (std::vector<WorkingPoint>::const_iterator it = wp.begin(); it != wp.end(); ++it)
        {
            edm::Handle<reco::JetTagCollection > jetTags;
            iEvent.getByLabel((*it).inputTag(),jetTags);
            std::string alias = (*it).alias();

            std::string moduleLabel = (jetTags).provenance()->moduleLabel();
            if (mymap.find(moduleLabel) != mymap.end()) continue;
            mymap[moduleLabel] = true;

            ith_tagged = TaggedJet(*jet,jetTags);

            if (ith_tagged == -1) continue;

            TaggerPerformances_[alias].Add( (*jetTags)[ith_tagged].second, JetFlavor );
        }
        jetIndex++;
    }

}

//define this as a plug-in
DEFINE_FWK_MODULE(OperatingPoints);
