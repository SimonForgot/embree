// ======================================================================== //
// Copyright 2009-2017 Intel Corporation                                    //
//                                                                          //
// Licensed under the Apache License, Version 2.0 (the "License");          //
// you may not use this file except in compliance with the License.         //
// You may obtain a copy of the License at                                  //
//                                                                          //
//     http://www.apache.org/licenses/LICENSE-2.0                           //
//                                                                          //
// Unless required by applicable law or agreed to in writing, software      //
// distributed under the License is distributed on an "AS IS" BASIS,        //
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. //
// See the License for the specific language governing permissions and      //
// limitations under the License.                                           //
// ======================================================================== //

#pragma once

#include "../common/default.h"
#include "../common/primref.h"
#include "../common/primref_mb.h"

namespace embree
{
  namespace isa
  {
    template<typename BBox>
      class CentGeom
    {
    public:
      __forceinline CentGeom () {}

      __forceinline CentGeom (EmptyTy) 
	: geomBounds(empty), centBounds(empty) {}
      
      __forceinline CentGeom (const BBox& geomBounds, const BBox3fa& centBounds) 
	: geomBounds(geomBounds), centBounds(centBounds) {}
      
      __forceinline void extend(const BBox& geomBounds_, const BBox3fa& centBounds_) {
	geomBounds.extend(geomBounds_);
	centBounds.extend(centBounds_);
      }

      __forceinline void reset() {
	geomBounds = empty;
	centBounds = empty;
      }

      template<typename PrimRef> 
        __forceinline void extend_primref(const PrimRef& prim) 
      {
        BBox bounds; Vec3fa center;
        prim.binBoundsAndCenter(bounds,center);
        geomBounds.extend(bounds);
        centBounds.extend(center);
      }

      __forceinline void extend(const BBox& geomBounds_) {
	geomBounds.extend(geomBounds_);
	centBounds.extend(center2(geomBounds_));
      }

      __forceinline void merge(const CentGeom& other) 
      {
	geomBounds.extend(other.geomBounds);
	centBounds.extend(other.centBounds);
      }
      
    public:
      BBox geomBounds;   //!< geometry bounds of primitives
      BBox3fa centBounds;   //!< centroid bounds of primitives
    };

    typedef CentGeom<BBox3fa> CentGeomBBox3fa;

    /*! stores bounding information for a set of primitives */
    template<typename BBox>
      class PrimInfoT : public CentGeom<BBox>
    {
    public:
      using CentGeom<BBox>::geomBounds;
      using CentGeom<BBox>::centBounds;

      __forceinline PrimInfoT () {}

      __forceinline PrimInfoT (EmptyTy) 
	: CentGeom<BBox>(empty), begin(0), end(0) {}

      __forceinline void reset() {
	CentGeom<BBox>::reset();
	begin = end = 0;
      }
      
      __forceinline PrimInfoT (size_t num, const BBox& geomBounds, const BBox3fa& centBounds) 
	: CentGeom<BBox>(geomBounds,centBounds), begin(0), end(num) {}
      
      __forceinline PrimInfoT (size_t begin, size_t end, const BBox& geomBounds, const BBox3fa& centBounds) 
	: CentGeom<BBox>(geomBounds,centBounds), begin(begin), end(end) {}

      template<typename PrimRef> 
        __forceinline void add_primref(const PrimRef& prim) 
      {
        CentGeom<BBox>::extend_primref(prim);
        end++;
      }

      __forceinline void add(const BBox& geomBounds_) {
	CentGeom<BBox>::extend(geomBounds_);
	end++;
      }

      __forceinline void add(const BBox& geomBounds_, const size_t i) {
	CentGeom<BBox>::extend(geomBounds_,center2(geomBounds_));
	end+=i;
      }

      __forceinline void add(const size_t i=1) {
	end+=i;
      }
       
      __forceinline void add(const BBox& geomBounds_, const BBox3fa& centBounds_, size_t num_ = 1) {
	CentGeom<BBox>::extend(geomBounds_,centBounds_);
	end += num_;
      }

      __forceinline void merge(const PrimInfoT& other) 
      {
	CentGeom<BBox>::merge(other);
	//assert(begin == 0);
        begin += other.begin;
	end += other.end;
      }

      static __forceinline const PrimInfoT merge(const PrimInfoT& a, const PrimInfoT& b) {
        PrimInfoT r = a; r.merge(b); return r;
      }
      
      /*! returns the number of primitives */
      __forceinline size_t size() const { 
	return end-begin; 
      }

      __forceinline float halfArea() {
        return expectedApproxHalfArea(geomBounds);
      }

      __forceinline float leafSAH() const { 
	return expectedApproxHalfArea(geomBounds)*float(size()); 
	//return halfArea(geomBounds)*blocks(num); 
      }
      
      __forceinline float leafSAH(size_t block_shift) const { 
	return expectedApproxHalfArea(geomBounds)*float((size()+(size_t(1)<<block_shift)-1) >> block_shift);
	//return halfArea(geomBounds)*float((num+3) >> 2);
	//return halfArea(geomBounds)*blocks(num); 
      }
      
      /*! stream output */
      friend std::ostream& operator<<(std::ostream& cout, const PrimInfoT& pinfo) {
	return cout << "PrimInfo { begin = " << pinfo.begin << ", end = " << pinfo.end << ", geomBounds = " << pinfo.geomBounds << ", centBounds = " << pinfo.centBounds << "}";
      }
      
    public:
      size_t begin,end;          //!< number of primitives
    };

    typedef PrimInfoT<BBox3fa> PrimInfo;
    //typedef PrimInfoT<LBBox3fa> PrimInfoMB;

    /*! stores bounding information for a set of primitives */
    template<typename BBox>
      class PrimInfoMBT : public CentGeom<BBox>
    {
    public:
      using CentGeom<BBox>::geomBounds;
      using CentGeom<BBox>::centBounds;

      __forceinline PrimInfoMBT () {
      } 

      __forceinline PrimInfoMBT (EmptyTy)
        : CentGeom<BBox>(empty), object_range(0,0), num_time_segments(0), max_num_time_segments(0), time_range(0.0f,1.0f) {}

      __forceinline PrimInfoMBT (size_t begin, size_t end)
        : CentGeom<BBox>(empty), object_range(begin,end), num_time_segments(0), max_num_time_segments(0), time_range(0.0f,1.0f) {}

      template<typename PrimRef> 
        __forceinline void add_primref(const PrimRef& prim) 
      {
        CentGeom<BBox>::extend_primref(prim);
        object_range._end++;
        num_time_segments += prim.size();
        max_num_time_segments = max(max_num_time_segments,size_t(prim.totalTimeSegments()));
      }

      __forceinline void merge(const PrimInfoMBT& other)
      {
        CentGeom<BBox>::merge(other);
        object_range._begin += other.object_range.begin();
	object_range._end += other.object_range.end();
        num_time_segments += other.num_time_segments;
        max_num_time_segments = max(max_num_time_segments,other.max_num_time_segments);
      }

      static __forceinline const PrimInfoMBT merge2(const PrimInfoMBT& a, const PrimInfoMBT& b) {
        PrimInfoMBT r = a; r.merge(b); return r;
      }
      
      /*! returns the number of primitives */
      __forceinline size_t size() const { 
	return object_range.size(); 
      }

      __forceinline float halfArea() const {
        return time_range.size()*expectedApproxHalfArea(geomBounds);
      }

      __forceinline float leafSAH() const { 
	return time_range.size()*expectedApproxHalfArea(geomBounds)*float(num_time_segments); 
	//return halfArea(geomBounds)*blocks(num); 
      }
      
      __forceinline float leafSAH(size_t block_shift) const { 
	return time_range.size()*expectedApproxHalfArea(geomBounds)*float((num_time_segments+(size_t(1)<<block_shift)-1) >> block_shift);
      }
      
      /*! stream output */
      friend std::ostream& operator<<(std::ostream& cout, const PrimInfoMBT& pinfo) 
      {
	return cout << "PrimInfo { " << 
          "object_range = " << pinfo.object_range << 
          ", time_range = " << pinfo.time_range << 
          ", time_segments = " << pinfo.num_time_segments << 
          ", geomBounds = " << pinfo.geomBounds << 
          ", centBounds = " << pinfo.centBounds << 
          "}";
      }
      
    public:
      range<size_t> object_range; //!< primitive range
      size_t num_time_segments;  //!< total number of time segments of all added primrefs
      size_t max_num_time_segments; //!< maximal number of time segments of a primitive
      BBox1f time_range;
    };

    typedef PrimInfoMBT<typename PrimRefMB::BBox> PrimInfoMB;

     struct SetMB
    {
      static const size_t PARALLEL_THRESHOLD = 3 * 1024;
      static const size_t PARALLEL_FIND_BLOCK_SIZE = 1024;
      static const size_t PARALLEL_PARTITION_BLOCK_SIZE = 128;

      typedef mvector<PrimRefMB>* PrimRefVector;

      __forceinline SetMB() {}
      
      __forceinline SetMB(PrimRefVector prims, range<size_t> object_range, BBox1f time_range)
        : prims(prims), object_range(object_range), time_range(time_range) {}
      
      __forceinline SetMB(PrimRefVector prims, BBox1f time_range = BBox1f(0.0f,1.0f))
        : prims(prims), object_range(range<size_t>(0,prims->size())), time_range(time_range) {}
      
      template<typename RecalculatePrimRef>
      __forceinline LBBox3fa linearBounds(const RecalculatePrimRef& recalculatePrimRef) const
      {
        auto reduce = [&](const range<size_t>& r) -> LBBox3fa
        {
          LBBox3fa cbounds(empty);
          for (size_t j = r.begin(); j < r.end(); j++)
          {
            PrimRefMB& ref = (*prims)[j];
            auto bn = recalculatePrimRef.linearBounds(ref, time_range);
            cbounds.extend(bn.first);
          };
          return cbounds;
        };
        
        return parallel_reduce(object_range.begin(), object_range.end(), PARALLEL_FIND_BLOCK_SIZE, PARALLEL_THRESHOLD, LBBox3fa(empty),
                               reduce,
                                   [&](const LBBox3fa& b0, const LBBox3fa& b1) -> LBBox3fa { return merge(b0, b1); });
      }
      
    public:
      PrimRefVector prims;
      range<size_t> object_range;
      BBox1f time_range;
    };
  }
}
