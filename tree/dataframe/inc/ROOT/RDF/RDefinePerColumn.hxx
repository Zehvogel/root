// Author: GitHub Copilot Agent, 2025

/*************************************************************************
 * Copyright (C) 1995-2025, Rene Brun and Fons Rademakers.               *
 * All rights reserved.                                                  *
 *                                                                       *
 * For the licensing terms see $ROOTSYS/LICENSE.                         *
 * For the list of contributors see $ROOTSYS/README/CREDITS.             *
 *************************************************************************/

#ifndef ROOT_RDF_RDEFINEMULTI
#define ROOT_RDF_RDEFINEMULTI

#include "ROOT/RDF/ColumnReaderUtils.hxx"
#include "ROOT/RDF/RColumnReaderBase.hxx"
#include "ROOT/RDF/RDefineBase.hxx"
#include "ROOT/RDF/RLoopManager.hxx"
#include "ROOT/RDF/Utils.hxx"
#include <string_view>
#include "ROOT/TypeTraits.hxx"
#include "RtypesCore.h"

#include <array>
#include <deque>
#include <memory>
#include <type_traits>
#include <utility> // std::index_sequence
#include <vector>

class TTreeReader;

namespace ROOT {
namespace Detail {
namespace RDF {

using namespace ROOT::TypeTraits;

/// Helper to extract the column type from an RVec return type
template <typename VaryExpressionRet_t>
struct ColumnTypeForMultiDefine {
};

template <typename T>
struct ColumnTypeForMultiDefine<ROOT::RVec<T>> {
   using type = T;
};

template <typename Ret_t>
using ColumnTypeForMultiDefine_t = typename ColumnTypeForMultiDefine<Ret_t>::type;

/// Shared state for a group of RDefine objects that are defined together
template <typename F, typename ExtraArgsTag>
class RDefineMultiSharedState {
   // shortcuts
   using NoneTag = ExtraArgsForDefine::None;
   using SlotTag = ExtraArgsForDefine::Slot;
   using SlotAndEntryTag = ExtraArgsForDefine::SlotAndEntry;
   // other types
   using FunParamTypes_t = typename CallableTraits<F>::arg_types;
   using ColumnTypesTmp_t =
      RDFInternal::RemoveFirstParameterIf_t<std::is_same<ExtraArgsTag, SlotTag>::value, FunParamTypes_t>;
   using ColumnTypes_t =
      RDFInternal::RemoveFirstTwoParametersIf_t<std::is_same<ExtraArgsTag, SlotAndEntryTag>::value, ColumnTypesTmp_t>;
   using TypeInd_t = std::make_index_sequence<ColumnTypes_t::list_size>;
   using Ret_t = typename CallableTraits<F>::ret_type;
   using DefinedCol_t = ColumnTypeForMultiDefine_t<Ret_t>;
   
   // Avoid instantiating vector<bool> as `operator[]` returns temporaries in that case. Use std::deque instead.
   using ValuesPerSlot_t =
      std::conditional_t<std::is_same<DefinedCol_t, bool>::value, std::deque<ROOT::RVec<DefinedCol_t>>, 
                         std::vector<ROOT::RVec<DefinedCol_t>>>;

public:
   F fExpression;
   ValuesPerSlot_t fLastResults; // Per-slot storage for all column values
   std::vector<Long64_t> fLastCheckedEntry;
   const std::vector<std::string> fColNames;
   const ROOT::RDF::ColumnNames_t fInputColumnNames;
   
   /// Column readers per slot and per input column
   std::vector<std::array<RColumnReaderBase *, ColumnTypes_t::list_size>> fValues;
   
   /// Flag to track whether InitSlot has been called for each slot
   std::vector<bool> fSlotInitialized;
   
   RDefineMultiSharedState(F expression, const std::vector<std::string> &colNames,
                          const ROOT::RDF::ColumnNames_t &inputColumns, unsigned int nSlots)
      : fExpression(std::move(expression)),
        fLastResults(nSlots * RDFInternal::CacheLineStep<ROOT::RVec<DefinedCol_t>>()),
        fLastCheckedEntry(nSlots * RDFInternal::CacheLineStep<Long64_t>(), -1),
        fColNames(colNames),
        fInputColumnNames(inputColumns),
        fValues(nSlots),
        fSlotInitialized(nSlots, false)
   {
      // Initialize storage for each slot
      for (auto i = 0u; i < nSlots; ++i) {
         fLastResults[i * RDFInternal::CacheLineStep<ROOT::RVec<DefinedCol_t>>()].resize(colNames.size());
      }
   }
   
   template <typename ColType>
   auto GetValueChecked(unsigned int slot, std::size_t readerIdx, Long64_t entry, 
                       const std::vector<std::string> &inputColNames) -> ColType &
   {
      if (auto *val = fValues[slot][readerIdx]->template TryGet<ColType>(entry))
         return *val;

      throw std::out_of_range{"RDataFrame: Define could not retrieve value for column '" + inputColNames[readerIdx] +
                              "' for entry " + std::to_string(entry) +
                              ". You can use the DefaultValueFor operation to provide a default value, or "
                              "FilterAvailable/FilterMissing to discard/keep entries with missing values instead."};
   }

   template <typename... ColTypes, std::size_t... S>
   void UpdateHelper(unsigned int slot, Long64_t entry, TypeList<ColTypes...>, std::index_sequence<S...>, NoneTag)
   {
      auto &&results = fExpression(GetValueChecked<ColTypes>(slot, S, entry, fInputColumnNames)...);
      
      if (results.size() != fColNames.size()) {
         throw std::runtime_error("The Define expression for multiple columns returned " + 
                                  std::to_string(results.size()) + " values, but " +
                                  std::to_string(fColNames.size()) + " were expected.");
      }
      
      // Move results to fLastResults
      fLastResults[slot * RDFInternal::CacheLineStep<ROOT::RVec<DefinedCol_t>>()] = std::move(results);
   }

   template <typename... ColTypes, std::size_t... S>
   void UpdateHelper(unsigned int slot, Long64_t entry, TypeList<ColTypes...>, std::index_sequence<S...>, SlotTag)
   {
      auto &&results = fExpression(slot, GetValueChecked<ColTypes>(slot, S, entry, fInputColumnNames)...);
      
      if (results.size() != fColNames.size()) {
         throw std::runtime_error("The Define expression for multiple columns returned " + 
                                  std::to_string(results.size()) + " values, but " +
                                  std::to_string(fColNames.size()) + " were expected.");
      }
      
      // Move results to fLastResults
      fLastResults[slot * RDFInternal::CacheLineStep<ROOT::RVec<DefinedCol_t>>()] = std::move(results);
   }

   template <typename... ColTypes, std::size_t... S>
   void UpdateHelper(unsigned int slot, Long64_t entry, TypeList<ColTypes...>, std::index_sequence<S...>, SlotAndEntryTag)
   {
      auto &&results = fExpression(slot, entry, GetValueChecked<ColTypes>(slot, S, entry, fInputColumnNames)...);
      
      if (results.size() != fColNames.size()) {
         throw std::runtime_error("The Define expression for multiple columns returned " + 
                                  std::to_string(results.size()) + " values, but " +
                                  std::to_string(fColNames.size()) + " were expected.");
      }
      
      // Move results to fLastResults
      fLastResults[slot * RDFInternal::CacheLineStep<ROOT::RVec<DefinedCol_t>>()] = std::move(results);
   }
   
   void Update(unsigned int slot, Long64_t entry)
   {
      if (entry != fLastCheckedEntry[slot * RDFInternal::CacheLineStep<Long64_t>()]) {
         // evaluate the define expression, cache the result
         UpdateHelper(slot, entry, ColumnTypes_t{}, TypeInd_t{}, ExtraArgsTag{});
         fLastCheckedEntry[slot * RDFInternal::CacheLineStep<Long64_t>()] = entry;
      }
   }
};

/// RDefinePerColumn wraps a single column from a multi-column Define
/// It shares the expression evaluation state with other columns defined by the same Define call
template <typename F, typename ExtraArgsTag = ExtraArgsForDefine::None>
class R__CLING_PTRCHECK(off) RDefinePerColumn final : public RDefineBase {
   using FunParamTypes_t = typename CallableTraits<F>::arg_types;
   using ColumnTypesTmp_t =
      RDFInternal::RemoveFirstParameterIf_t<std::is_same<ExtraArgsTag, ExtraArgsForDefine::Slot>::value, FunParamTypes_t>;
   using ColumnTypes_t =
      RDFInternal::RemoveFirstTwoParametersIf_t<std::is_same<ExtraArgsTag, ExtraArgsForDefine::SlotAndEntry>::value, ColumnTypesTmp_t>;
   using TypeInd_t = std::make_index_sequence<ColumnTypes_t::list_size>;
   using Ret_t = typename CallableTraits<F>::ret_type;
   using DefinedCol_t = ColumnTypeForMultiDefine_t<Ret_t>;
   
   std::shared_ptr<RDefineMultiSharedState<F, ExtraArgsTag>> fSharedState;
   const std::size_t fColumnIndex; // Index of this column in the multi-column define
   
   /// Define objects corresponding to systematic variations other than nominal for this defined column.
   std::unordered_map<std::string, std::unique_ptr<RDefineBase>> fVariedDefines;

public:
   RDefinePerColumn(std::string_view name, std::string_view type, 
                   std::shared_ptr<RDefineMultiSharedState<F, ExtraArgsTag>> sharedState,
                   std::size_t columnIndex,
                   const RDFInternal::RColumnRegister &colRegister, RLoopManager &lm,
                   const std::string &variationName = "nominal")
      : RDefineBase(name, type, colRegister, lm, sharedState->fInputColumnNames, variationName),
        fSharedState(sharedState),
        fColumnIndex(columnIndex)
   {
      fLoopManager->Register(this);
   }

   RDefinePerColumn(const RDefinePerColumn &) = delete;
   RDefinePerColumn &operator=(const RDefinePerColumn &) = delete;
   ~RDefinePerColumn() { fLoopManager->Deregister(this); }

   void InitSlot(TTreeReader *r, unsigned int slot) final
   {
      // Only initialize column readers once per slot (thread-safe since each slot is accessed by only one thread)
      if (!fSharedState->fSlotInitialized[slot]) {
         RDFInternal::RColumnReadersInfo info{fSharedState->fInputColumnNames, fColRegister, fIsDefine.data(), *fLoopManager};
         fSharedState->fValues[slot] = RDFInternal::GetColumnReaders(slot, r, ColumnTypes_t{}, info, fVariation);
         fSharedState->fSlotInitialized[slot] = true;
      }
      fLastCheckedEntry[slot * RDFInternal::CacheLineStep<Long64_t>()] = -1;
   }

   /// Return the (type-erased) address of the Define'd value for the given processing slot.
   void *GetValuePtr(unsigned int slot) final
   {
      return static_cast<void *>(&fSharedState->fLastResults[slot * RDFInternal::CacheLineStep<ROOT::RVec<DefinedCol_t>>()][fColumnIndex]);
   }

   /// Update the value at the address returned by GetValuePtr with the content corresponding to the given entry
   void Update(unsigned int slot, Long64_t entry) final
   {
      // The shared state handles caching, so we can safely call Update multiple times
      fSharedState->Update(slot, entry);
      fLastCheckedEntry[slot * RDFInternal::CacheLineStep<Long64_t>()] = entry;
   }

   void Update(unsigned int /*slot*/, const ROOT::RDF::RSampleInfo &/*id*/) final {}

   const std::type_info &GetTypeId() const final { return typeid(DefinedCol_t); }

   /// Clean-up operations to be performed at the end of a task.
   void FinalizeSlot(unsigned int slot) final
   {
      // Clean up shared resources only once per slot
      // We use a simple check: only the first column (index 0) cleans up
      if (fColumnIndex == 0 && fSharedState->fSlotInitialized[slot]) {
         fSharedState->fValues[slot].fill(nullptr);
         fSharedState->fSlotInitialized[slot] = false;
      }

      for (auto &e : fVariedDefines)
         e.second->FinalizeSlot(slot);
   }

   /// Create clones of this Define that work with values in varied "universes".
   void MakeVariations(const std::vector<std::string> &variations) final
   {
      for (const auto &variation : variations) {
         if (std::find(fVariationDeps.begin(), fVariationDeps.end(), variation) == fVariationDeps.end()) {
            // this Defined quantity does not depend on this variation
            continue;
         }
         if (fVariedDefines.find(variation) != fVariedDefines.end())
            continue; // we already have this variation stored

         // Create a varied define with its own shared state (expression gets copied)
         auto variedSharedState = std::make_shared<RDefineMultiSharedState<F, ExtraArgsTag>>(
            fSharedState->fExpression, fSharedState->fColNames, fSharedState->fInputColumnNames, 
            fLoopManager->GetNSlots());
         
         auto variedDefine = std::unique_ptr<RDefineBase>(
            new RDefinePerColumn(fName, fType, variedSharedState, fColumnIndex, fColRegister, *fLoopManager, variation));
         fVariedDefines[variation] = std::move(variedDefine);
      }
   }

   /// Return a clone of this Define that works with values in the variationName "universe".
   RDefineBase &GetVariedDefine(const std::string &variationName) final
   {
      auto it = fVariedDefines.find(variationName);
      if (it == fVariedDefines.end()) {
         assert(std::find(fVariationDeps.begin(), fVariationDeps.end(), variationName) == fVariationDeps.end());
         return *this;
      }

      return *(it->second);
   }
};

} // ns RDF
} // ns Detail
} // ns ROOT

#endif // ROOT_RDF_RDEFINEMULTI
