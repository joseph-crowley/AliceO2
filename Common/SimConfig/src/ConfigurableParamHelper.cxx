// Copyright CERN and copyright holders of ALICE O2. This software is
// distributed under the terms of the GNU General Public License v3 (GPL
// Version 3), copied verbatim in the file "COPYING".
//
// See http://alice-o2.web.cern.ch/license for full licensing information.
//
// In applying this license CERN does not waive the privileges and immunities
// granted to it by virtue of its status as an Intergovernmental Organization
// or submit itself to any jurisdiction.

//first version 8/2018, Sandro Wenzel

#include "SimConfig/ConfigurableParamHelper.h"
#include "SimConfig/ConfigurableParam.h"
#include <TClass.h>
#include <TDataMember.h>
#include <TDataType.h>
#include <TIterator.h>
#include <TList.h>
#include <iostream>
#include "FairLogger.h"
#include <boost/property_tree/ptree.hpp>
#include <functional>

using namespace o2::conf;

// a generic looper of data members of a TClass; calling a callback
// reused in various functions below
void loopOverMembers(TClass* cl, void* obj,
                     std::function<void(const TDataMember*, int, int)>&& callback)
{
  auto memberlist = cl->GetListOfDataMembers();
  for (int i = 0; i < memberlist->GetEntries(); ++i) {
    auto dm = (TDataMember*)memberlist->At(i);

    // filter out static members for now
    if (dm->Property() & kIsStatic) {
      continue;
    }
    if (dm->IsaPointer()) {
      LOG(WARNING) << "Pointer types not supported in ConfigurableParams";
      continue;
    }
    if (!dm->IsBasic()) {
      LOG(WARNING) << "Complex types not supported in ConfigurableParams";
      continue;
    }
    const auto dim = dm->GetArrayDim();
    // we support very simple vectored data in 1D for now
    if (dim > 1) {
      LOG(WARNING) << "We support at most 1 dimensional arrays in ConfigurableParams";
      continue;
    }
    const auto size = (dim == 1) ? dm->GetMaxIndex(dim - 1) : 1; // size of array (1 if scalar)
    for (int index = 0; index < size; ++index) {
      callback(dm, index, size);
    }
  }
}

// construct name (in dependence on vector or scalar data and index)
std::string getName(const TDataMember* dm, int index, int size)
{
  std::stringstream namestream;
  namestream << dm->GetName();
  if (size > 1) {
    namestream << "[" << index << "]";
  }
  return namestream.str();
}

void _ParamHelper::printParametersImpl(std::string mainkey, TClass* cl, void* obj,
                                       std::map<std::string, ConfigurableParam::EParamProvenance> const* provmap)
{
  auto printMembers = [&mainkey, obj, provmap](const TDataMember* dm, int index, int size) {
    // pointer to object
    auto dt = dm->GetDataType();
    char* pointer = ((char*)obj) + dm->GetOffset() + index * dt->Size();
    const auto name = getName(dm, index, size);
    std::cout << name << " : " << dt->AsString(pointer);
    if (provmap != nullptr) {
      auto iter = provmap->find(mainkey + "." + name);
      if (iter != provmap->end()) {
        std::cout << "\t\t[ " << ConfigurableParam::toString(iter->second) << " ]";
      }
    }
    std::cout << "\n";
  };
  loopOverMembers(cl, obj, printMembers);
}

void _ParamHelper::fillKeyValuesImpl(std::string mainkey, TClass* cl, void* obj, boost::property_tree::ptree* tree,
                                     std::map<std::string, std::pair<int, void*>>* keytostoragemap)
{
  boost::property_tree::ptree localtree;
  auto fillMap = [obj, &mainkey, &localtree, &keytostoragemap](const TDataMember* dm, int index, int size) {
    const auto name = getName(dm, index, size);
    auto dt = dm->GetDataType();
    char* pointer = ((char*)obj) + dm->GetOffset() + index * dt->Size();
    localtree.put(name, dt->AsString(pointer));

    auto key = mainkey + "." + name;
    using mapped_t = std::pair<int, void*>;
    keytostoragemap->insert(std::pair<std::string, mapped_t>(key, mapped_t(dt->GetType(), pointer)));
  };
  loopOverMembers(cl, obj, fillMap);
  tree->add_child(mainkey, localtree);
}

bool isMemblockDifferent(char const* block1, char const* block2, int sizeinbytes)
{
  // loop over thing in elements of bytes
  for (int i = 0; i < sizeinbytes / sizeof(char); ++i) {
    if (block1[i] != block2[i]) {
      return false;
    }
  }
  return true;
}

void _ParamHelper::assignmentImpl(std::string mainkey, TClass* cl, void* to, void* from,
                                  std::map<std::string, ConfigurableParam::EParamProvenance>* provmap)
{
  auto assignifchanged = [to, from, &mainkey, provmap](const TDataMember* dm, int index, int size) {
    const auto name = getName(dm, index, size);
    auto dt = dm->GetDataType();
    char* pointerto = ((char*)to) + dm->GetOffset() + index * dt->Size();
    char* pointerfrom = ((char*)from) + dm->GetOffset() + index * dt->Size();
    if (!isMemblockDifferent(pointerto, pointerfrom, dt->Size())) {
      auto key = mainkey + "." + name;
      auto iter = provmap->find(key);
      if (iter != provmap->end()) {
        iter->second = ConfigurableParam::EParamProvenance::kCCDB; // TODO: change to "current STATE"??
      } else {
        LOG(WARN) << "KEY " << key << " NOT FOUND WHILE UPDATING PARAMETER PROVENANCE";
      }
      // actually copy
      std::memcpy(pointerto, pointerfrom, dt->Size());
    }
  };
  loopOverMembers(cl, to, assignifchanged);
}

void _ParamHelper::printWarning(std::type_info const& tinfo)
{
  LOG(WARNING) << "Registered parameter class with name " << tinfo.name()
               << " has no ROOT dictionary and will not be available in the configurable parameter system";
}