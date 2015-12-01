#!/usr/bin/python

import argparse

from ROOT import gROOT

gROOT.ProcessLine(".L $HOMUONTRIGGER_BASE/python/loader.C+");


from makeControlPlots import ControlPlots
from makeEvsEtaPhiPlot import EvsEtaPhi
from phishift.DeltaPhi import DeltaPhi
from efficiency.TimeWindow import TimeWindow
from dataQuality.PtResolution import PtResolution
from dataQuality.QualityCode import QualityCode
from dataQuality.Timing import Timing

parser = argparse.ArgumentParser()
parser.add_argument('scripts', metavar='scripts', type=str, nargs='+',
                   help='The subscript(s) to be called for plotting')

parser.add_argument('--data','-d'
					,dest='data'
					,action="store_true",default=False
					,help='Skip anything that has to do with GEN')

parser.add_argument('--source','-s'
					,dest='source'
					,help='File name scheme for the source of events')

args = parser.parse_args()

for script in args.scripts:
	if(script == 'controlPlots'):
		plots = ControlPlots(filename=args.source,data=args.data)
		res1 = plots.plotL1PerPt()
		res2 = plots.plotHoEtaPhi()
		res3 = plots.plotHoEtaPhiMatchedToL1()
		#res2 = plots.plotHoDigiMatchesPerDetId()
		if not args.data:
			res4 = plots.plotEfficiencyCountCheck()
			res5 = plots.plotGenEtaPhi()
		raw_input('-->')
	elif(script == 'eVsEtaPhi'):
		lib = EvsEtaPhi(filename = args.source, data=args.data)
		res = lib.plotAverageEnergyAroundL1()
		res2 = lib.plotAverageEMaxAroundL1()
		res3 = lib.plot1DEnergyAroundL1()
		res4 = lib.plot1DEMaxAroundL1()
		res5 = lib.compareHistogramMethods()
		raw_input('-->')
	elif(script == 'phiShift'):
		lib = DeltaPhi(filename = args.source,data=args.data)
		resAllEtaPhi = lib.plotEtaPhiForAllL1()
		resEtaPhiMap = lib.plotEtaPhiForDeltaPhiOne()
		resWheelwise = lib.plotEAveragePerWheel()
		resDeltaPhi = lib.plotDeltaPhiHistogram()
		resEta = lib.plotDeltaPhiVsL1Eta()
		if not args.data:
			#res3 = lib.plotDeltaPhiVsGenPt()
			res4 = lib.plotL1PhiVsHoPhi()
			res5 = lib.plotL1PhiVsHoIPhi()
			#res = lib.plotDeltaPhiVsL1Phi()
			#res2 = lib.plotDeltaPhiVsL1Pt()
		raw_input('-->')
	elif(script == 'timeWindow'):
		lib = TimeWindow(filename=args.source,data=args.data)
		resAllL1 = lib.plotAllL1Together()
		resBxidFail = lib.plotBxidVsPtFails()
		resBxidSucc = lib.plotBxidVsPtMatch()
		if not args.data:
			resTimeWindowAlone = lib.plotTimeWindowAlone()
			resTruthL1 = lib.plotTruthL1Together()
		raw_input('-->')
	elif(script == 'ptResolution'):
		lib = PtResolution(filename=args.source,data=args.data)
		res1 = lib.plotPtResolutionHistograms()
		raw_input('-->')
	elif(script == 'qualityCodes'):
		lib = QualityCode(filename=args.source,data=args.data)
		res1 = lib.plot3x3MatchQualityCodes()
		res2 = lib.plot3x3FailQualityCodes()
		raw_input('-->')
	elif (script=='timing'):
		lib = Timing(filename=args.source,data=args.data)
		resEvsTime = lib.plotHoEnergyVsTime()
		lib.plotDeltaTime()
		lib.plotL1BxId()
		res = lib.plotHoTime()
		res6 = lib.plotHoTimeLog()
		if not args.data:
			lib.plotEtaOfWrongBxId()
			lib.plotEtaPhiOfWrongBxId()
			lib.plotFractionsOfBxId()
			res2 = lib.plotDetectorContributionsToTiming()
			res5 = lib.plotPtAndPhiOfWrongBxId()
			res4 = lib.plotImprovementInDt()
			res3 = lib.plotPtAndEtaOfWrongBxId()
		raw_input('-->')
	else:
		print 'Unknown script requested: %s' % (script)