// Copyright 2010 Google Inc.
// 
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
// 
//      http://www.apache.org/licenses/LICENSE-2.0
// 
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
// ------------------------------------------------------------------------

template <typename T, typename SampleTraits> class SimpleWRS;

// An adapter between weighted-reservoir-sampler and the szl emitter.

class SzlWeightedSampleAdapter {
 public:
  // This class is provided so that all table entries can share one random
  // number generator instance, and when the random number generator's reseeding
  // is transparent to the table entries (a proxy is a necessary hack because
  // a SimpleWRS instance cannot change its RandomBase* pointer).
  class RandomProxy : public RandomBase {
   public:
    // Takes ownership of real.
    explicit RandomProxy(RandomBase* real) : real_(real)  { }
    virtual ~RandomProxy()  { }

    // Simply forwards the call to real.
    virtual RandomProxy* Clone() const {
      RandomBase* real_copy = real_->Clone();
      return real_copy == NULL ? NULL : new RandomProxy(real_copy);
    }
    virtual uint8 Rand8() { return real_->Rand8(); }
    virtual uint16 Rand16() { return real_->Rand16(); }
    virtual uint32 Rand32() { return real_->Rand32(); }
    virtual uint64 Rand64() { return real_->Rand64(); }

    virtual string RandString(int desired_len) {
      return real_->RandString(desired_len);
    }
    virtual int32 UnbiasedUniform(int32 n) {
      return real_->UnbiasedUniform(n);
    }
    virtual uint64 UnbiasedUniform64(uint64 n) {
      return real_->UnbiasedUniform64(n);
    }

    // Takes ownership of real. It is the caller's responsibility to ensure that
    // there is no reference loop if real is also a RandomProxy.
    void reset(RandomBase* real) {
      delete real_;
      real_ = real;
    }

   private:
    RandomBase* real_;
  };

  // random must not be NULL. random is not owned by *this.
  // You won't be able to use another random number generator later, unless
  // random is a RandomProxy instance.
  SzlWeightedSampleAdapter(const SzlOps& weight_ops,
                           int maxElems, RandomBase* random) :
      sampler_(maxElems, random), weight_ops_(weight_ops), totElems_(0)  { }
  ~SzlWeightedSampleAdapter()  { }

  // Returns true iff the input table type is valid. error must not be NULL.
  static bool TableTypeValid(const SzlType& type, string* error);

  // Adds an element without specifying a weight (default to 1).
  int AddElem(const string& value) {
    return AddWeightedElemInternal(value, 1.0);
  }
  // Adds an element with a weight.
  int AddWeightedElem(const string& value, const SzlValue& weight) {
    double d;
    weight_ops_.ToFloat(weight, &d);
    return AddWeightedElemInternal(value, d);
  }

  // Number of candidate elements current held.
  int nElems() const  { return sampler_.current_sample_size(); }

  // Max. elements we ever hold.
  int maxElems() const  { return sampler_.max_sample_size(); }

  // Report the total elements added to this entry in the table.
  int64 TotElems() const  { return totElems_; }

  // Return an unordered element.
  // REQUIRES 0 <= i < nElems().
  const string& Element(int i) const  { return sampler_.sample(i); }

  // Estimate memory in bytes currently allocated, excluding sizeof(*this).
  int ExtraMemory() const;

  // Clears all samples, as if no sample had been input.
  void Clear() {
    sampler_.clear();
    totElems_ = 0;
  }

  // Wipes out encoded and then outputs the information of all samples.
  // encoded must not be NULL.
  void Encode(string* encoded) const;

  // For SzlWeightedSampleEntry: clears output and then outputs the information
  // of all samples (one string per sample).
  void EncodeForDisplay(vector<string>* output) const;

  // Merge the samples from an encoded string.
  // encoded must have been generated by Encode.
  // Returns true iff encoded is valid.
  bool Merge(const string& encoded);

  // For SzlWeightedSampleResults: divides the merged encoded string into a
  // vector of sub-strings (one per sample), without a SzlWeightedSampleAdapter
  // instance. The following calls are equivalent:
  //   A)
  //     vector<string> encoded_strs;
  //     string encoded;
  //     this->Encode(encoded);
  //     int64 dummy;
  //     SzlWeightedSampleAdapter::SplitEncodedStr(
  //         this->ops(), encoded, this->maxElems(), &encoded_strs, &dummy);
  //   B)
  //     vector<string> encoded_strs;
  //     this->EncodeForDisplay(&encoded_strs);
  // Returns true iff encoded is valid.
  // encoded must have been generated by Encode. max_elems must equal
  // maxElems() of the SzlWeightedSampleAdapter instance that encoded the input
  // string. total_elems must not be NULL.
  static bool SplitEncodedStr(const string& encoded, int max_elems,
                              vector<string>* output, int64* total_elems);
  bool IsValid() const {
    // It is possible that nElems() < maxElems() && nElems() < TotElems()),
    // because samples with negative weights will contribute to TotElems()
    // but not nElems().
    return nElems() <= maxElems() && nElems() <= TotElems();
  }

 private:
  // Used in element assignment.
  struct ElemSrc {
    const string* value;
    int delta_memory;
  };

  // Template argument of SimpleWRS; used in element assignment.
  struct ElemTraits {
    typedef ElemSrc* SrcType;
    static void SetSample(SrcType src, string* dest) {
      // sampler_.extra_memory() does not change over time
      src->delta_memory = src->value->size() - dest->size();
      *dest = *src->value;
    }
  };

  int AddWeightedElemInternal(const string& value, double weight) {
    ++totElems_;
    ElemSrc src = {&value, 0};
    sampler_.ConsiderSample(weight, &src);
    return src.delta_memory;
  }

  // Return the tag of an unordered element.
  // REQUIRES 0 <= i < nElems().
  double ElementTag(int i) const {
    return sampler_.key(i);
  }

  SimpleWRS<string, ElemTraits> sampler_;
  const SzlOps& weight_ops_;
  // total elements every added to the table
  int64 totElems_;
};
