// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

#define BOOST_TEST_MODULE Test InteractionSampler class
#define BOOST_TEST_MAIN
#define BOOST_TEST_DYN_LINK
#include <algorithm>
#include <bitset>
#include <boost/test/unit_test.hpp>
#include "Steer/InteractionSampler.h"
#include "Steer/HBFSampler.h"
#include "Headers/RAWDataHeader.h"
#include <TRandom.h>
#include <FairLogger.h>

// @brief test and demo for HBF sampling for simulated IRs
// @author ruben.shahoyan@cern.ch

namespace o2
{
BOOST_AUTO_TEST_CASE(HBFSampler)
{
  using RDH = o2::header::RAWDataHeaderV5;
  using IR = o2::InteractionRecord;

  o2::steer::InteractionSampler irSampler;
  irSampler.setInteractionRate(12000); // ~1.5 interactions per orbit
  irSampler.init();

  int nIRs = 500;
  std::vector<o2::InteractionTimeRecord> irs(nIRs);
  irSampler.generateCollisionTimes(irs);

  LOG(INFO) << "Emulate RDHs for raw data between IRs " << irs.front() << " and " << irs.back();

  // default sampler with BC filling like in TPC TDR, 50kHz
  o2::steer::HBFSampler sampler;

  uint8_t packetCounter = 0;
  std::vector<o2::InteractionRecord> HBIRVec;
  auto irFrom = sampler.getFirstIR(); // TFs are counted from this IR
  int nHBF = 0, nHBFEmpty = 0, nTF = 0;
  int nHBFOpen = 0, nHBFClose = 0;
  RDH rdh;
  IR rdhIR;
  auto flushRDH = [&]() {
    bool empty = rdh.offsetToNext == sizeof(RDH);
    std::bitset<32> trig(rdh.triggerType);
    int hbfID = sampler.getHBF(rdhIR);
    auto tfhb = sampler.getTFandHBinTF(rdhIR);

    printf("%s HBF%4d (TF%3d/HB%3d) Sz:%4d| HB Orbit/BC :%4d/%4d Trigger: %s Packet: %3d Page: %3d Stop: %d\n",
           rdh.stop ? "Close" : "Open ",
           hbfID, tfhb.first, tfhb.second, rdh.memorySize, rdhIR.orbit, rdhIR.bc, trig.to_string().c_str(),
           rdh.packetCounter, int(rdh.pageCnt), int(rdh.stop));
    if (rdh.stop) {
      nHBFClose++;
    } else {
      nHBFOpen++;
      if (rdh.triggerType & o2::trigger::TF) {
        nTF++;
      }
      if (rdh.triggerType & o2::trigger::HB) {
        nHBF++;
      }
      if (empty) {
        nHBFEmpty++;
      }
    }
  };

  for (int i = 0; i < nIRs; i++) {
    int nHBF = sampler.fillHBIRvector(HBIRVec, irFrom, irs[i]);
    irFrom = irs[i] + 1;

    // nHBF-1 HBframes don't have data, we need to create empty HBFs for them
    if (nHBF) {
      if (rdh.stop) { // do we need to close previous HBF?
        flushRDH();
      }
      for (int j = 0; j < nHBF - 1; j++) {
        rdhIR = HBIRVec[j];
        rdh = sampler.createRDH<RDH>(rdhIR);
        // dress rdh with cruID/FEE/Link ID ...
        rdh.packetCounter = packetCounter++;
        rdh.memorySize = sizeof(rdh);
        rdh.offsetToNext = sizeof(rdh);

        flushRDH(); // open empty HBH
        rdh.stop = 0x1;
        rdh.pageCnt++;
        flushRDH(); // close empty HBF
      }

      rdhIR = HBIRVec.back();
      rdh = sampler.createRDH<RDH>(rdhIR);
      rdh.packetCounter = packetCounter++;
      rdh.memorySize = sizeof(rdh) + 16 + gRandom->Integer(8192 - sizeof(rdh) - 16); // random payload
      rdh.offsetToNext = rdh.memorySize;
      flushRDH();     // open non-empty HBH
      rdh.stop = 0x1; // flag that it should be closed
      rdh.pageCnt++;
    }
    // flush payload
    printf("Flush payload for Orbit/BC %4d/%d\n", irs[i].orbit, irs[i].bc);
  }
  // close last packet
  if (rdh.stop) { // do we need to close previous HBF?
    flushRDH();
  } else {
    BOOST_CHECK(false); // lost closing RDH?
  }

  printf("\nN_TF=%d, N_HBF=%d (%d empty), Opened %d / Closed %d\n", nTF, nHBF, nHBFEmpty, nHBFOpen, nHBFClose);
  BOOST_CHECK(nHBF > nHBFEmpty);
  BOOST_CHECK(nTF > 0);
  BOOST_CHECK(nHBFOpen == nHBFClose);
}
} // namespace o2
