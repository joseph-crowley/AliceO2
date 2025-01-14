// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.
#include <gpucf/common/DataSet.h>
#include <gpucf/common/Digit.h>
#include <gpucf/common/log.h>
#include <gpucf/common/serialization.h>

#include <args/args.h>

#include <iostream>

using namespace gpucf;

int main(int argc, const char* argv[])
{
  args::ArgumentParser parser("");

  args::HelpFlag help(parser, "help", "Display help menu", {'h', "help"});
  args::ValueFlag<std::string> infile(parser, "IN", "Input file", {'i', "in"});
  args::ValueFlag<std::string> outfile(parser, "OUT", "Output file", {'o', "out"});

  try {
    parser.ParseCLI(argc, argv);
  } catch (const args::Help&) {
    std::cerr << parser;
    return 1;
  }

  log::Info() << "Reading digit file " << args::get(infile);

  DataSet data;
  data.read(args::get(infile));
  std::vector<Digit> digits = data.deserialize<Digit>();

  std::vector<RawDigit> rawDigits;
  for (const Digit& d : digits) {
    rawDigits.push_back({d.row, d.pad, d.time, d.charge});
  }

  log::Info() << "Writing binary digits to file " << args::get(outfile);
  gpucf::write<RawDigit>(args::get(outfile), rawDigits);

  return 0;
}

// vim: set ts=4 sw=4 sts=4 expandtab:
