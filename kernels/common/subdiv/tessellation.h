// ======================================================================== //
// Copyright 2009-2014 Intel Corporation                                    //
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

namespace embree
{
  /* int->float lookup table for single axis of a regular grid, grid dimension would be (2^l+1)*(2^l+1) */
  struct __aligned(64) RegularGridLookUpTables
  {
  private:
    const static size_t MAX_SUBDIVISION_LEVEL = 8;
    const static size_t MAX_TABLE_ENTRIES = 2*(((unsigned int)1 << MAX_SUBDIVISION_LEVEL)+1);
    unsigned int offset[MAX_SUBDIVISION_LEVEL];
    
    float table[ MAX_TABLE_ENTRIES ];
    
  public:
    
    __forceinline float lookUp(const size_t level,
			       const size_t index) const
    {
      assert(level < MAX_SUBDIVISION_LEVEL);
      assert(offset[level]+index < MAX_TABLE_ENTRIES); 
      return table[offset[level]+index];
    }
    
    RegularGridLookUpTables()
    {
      size_t index = 0;
      for (size_t l=0;l<MAX_SUBDIVISION_LEVEL;l++)
      {
	const unsigned int grid_size = (1 << l) + 1;
	offset[l] = index;
	for (size_t i=0;i<grid_size;i++)
	  table[index++] = (float)i / (float)(grid_size-1);
      }	
      assert(index < MAX_TABLE_ENTRIES );
      
      /* for (size_t l=0;l<MAX_SUBDIVISION_LEVEL;l++) */
      /*   { */
      /*     DBG_PRINT(l); */
      /*     const unsigned int grid_size = (1 << l) + 1; */
      /*     DBG_PRINT(grid_size); */
      
      /*     for (size_t i=0;i<grid_size;i++) */
      /*       { */
      /* 	DBG_PRINT(i); */
      /* 	DBG_PRINT(lookUp(l,i)); */
      /*       } */
      /*   }	 */
    }
  };
  
  /* old buggy version */
  __forceinline void stichEdges(const unsigned int low_rate_segments,
				const unsigned int high_rate_segments,
				float * __restrict__ const uv_array,
				const unsigned int uv_array_step)
  {
    assert(low_rate_segments < high_rate_segments);
    assert(high_rate_segments >= 2);
    
    const float inv_low_rate_segments = 1.0f / (float)low_rate_segments;
    const unsigned int high_rate_points = high_rate_segments+1;
    const unsigned int dy = low_rate_segments+1; // [0,..,low_rate_segments]   
    const unsigned int dx = high_rate_segments-1;
    
    int p = 2*dy-dx;  
    
    unsigned int offset = uv_array_step;
    
    for(unsigned int x=1, y=0; x<high_rate_segments; x++) // inner points [1,..,n-1]
    {
      uv_array[offset] = (float)y * inv_low_rate_segments;
      
      offset += uv_array_step;      
      if(p > 0)
      {
	y++;
	p -= 2*dx;
      }
      p += 2*dy;
    }
  }
  
  
  __forceinline void stichGridEdges(const unsigned int low_rate,
				    const unsigned int high_rate,
				    float * __restrict__ const uv_array,
				    const unsigned int uv_array_step)
  {
    assert(low_rate < high_rate);
    assert(high_rate >= 2);
    
    const float inv_low_rate = 1.0f / (float)(low_rate-1);
    const unsigned int dy = low_rate  - 1; 
    const unsigned int dx = high_rate - 1;
    
    int p = 2*dy-dx;  
    
    unsigned int offset = 0;
    unsigned int y = 0;
    float value = 0.0f;
    for(unsigned int x=0;x<high_rate-1; x++) // '<=' would be correct but we will leave the 1.0f at the end
    {
      uv_array[offset] = value;
      
      offset += uv_array_step;      
      if(unlikely(p > 0))
      {
	y++;
	value = (float)y * inv_low_rate;
	p -= 2*dx;
      }
      p += 2*dy;
    }
  }
  
  __forceinline void stichUVGrid(const float edge_levels[4],
				 const unsigned int grid_u_res,
				 const unsigned int grid_v_res,
				 float * __restrict__ const u_array,
				 float * __restrict__ const v_array)
  {
    const unsigned int int_edge_points0 = (unsigned int)edge_levels[0] + 1;
    const unsigned int int_edge_points1 = (unsigned int)edge_levels[1] + 1;
    const unsigned int int_edge_points2 = (unsigned int)edge_levels[2] + 1;
    const unsigned int int_edge_points3 = (unsigned int)edge_levels[3] + 1;
    
    if (unlikely(int_edge_points0 < grid_u_res))
      stichGridEdges(int_edge_points0,grid_u_res,u_array,1);
    
    if (unlikely(int_edge_points2 < grid_u_res))
      stichGridEdges(int_edge_points2,grid_u_res,&u_array[(grid_v_res-1)*grid_u_res],1);
    
    if (unlikely(int_edge_points1 < grid_v_res))
      stichGridEdges(int_edge_points1,grid_v_res,&v_array[grid_u_res-1],grid_u_res);
    
    if (unlikely(int_edge_points3 < grid_v_res))
      stichGridEdges(int_edge_points3,grid_v_res,v_array,grid_u_res);  
  }
  
  __forceinline void gridUVTessellator(const float edge_levels[4],
				       const unsigned int grid_u_res,
				       const unsigned int grid_v_res,
				       float * __restrict__ const u_array,
				       float * __restrict__ const v_array)
  {
    assert( grid_u_res >= 1);
    assert( grid_v_res >= 1);
    assert( edge_levels[0] >= 1.0f );
    assert( edge_levels[1] >= 1.0f );
    assert( edge_levels[2] >= 1.0f );
    assert( edge_levels[3] >= 1.0f );
    
    const unsigned int grid_u_segments = grid_u_res-1;
    const unsigned int grid_v_segments = grid_v_res-1;
    
    const float inv_grid_u_segments = 1.0f / grid_u_segments;
    const float inv_grid_v_segments = 1.0f / grid_v_segments;
    
    /* initialize grid */
    unsigned int index = 0;
    for (unsigned int y=0;y<grid_v_res;y++)
    {
      const float v = (float)y * inv_grid_v_segments;
      for (unsigned int x=0;x<grid_u_res;x++,index++)
      {
	u_array[index] = (float)x * inv_grid_u_segments;
	v_array[index] = v;
      }
    }
    const unsigned int num_points = index;
    
    /* set right and buttom border to exactly 1.0f */
    for (unsigned int y=0,i=grid_u_res-1;y<grid_v_res;y++,i+=grid_u_res)
      u_array[i] = 1.0f;
    for (unsigned int x=0;x<grid_u_res;x++)
      v_array[num_points-1-x] = 1.0f;
    
    
    /* stich different tessellation levels in u/v grid */
    stichUVGrid(edge_levels,grid_u_res,grid_v_res,u_array,v_array);
  }
  
#if defined(__MIC__)
  
  __forceinline void gridUVTessellatorMIC(const float edge_levels[4],
					  const unsigned int grid_u_res,
					  const unsigned int grid_v_res,
					  float * __restrict__ const u_array,
					  float * __restrict__ const v_array)
  {
    assert( grid_u_res >= 1);
    assert( grid_v_res >= 1);
    assert( edge_levels[0] >= 1.0f );
    assert( edge_levels[1] >= 1.0f );
    assert( edge_levels[2] >= 1.0f );
    assert( edge_levels[3] >= 1.0f );
    
    const mic_i grid_u_segments = mic_i(grid_u_res)-1;
    const mic_i grid_v_segments = mic_i(grid_v_res)-1;
    
    const mic_f inv_grid_u_segments = rcp(mic_f(grid_u_segments));
    const mic_f inv_grid_v_segments = rcp(mic_f(grid_v_segments));
    
    unsigned int index = 0;
    mic_i v_i( zero );
    for (unsigned int y=0;y<grid_v_res;y++,index+=grid_u_res,v_i += 1)
    {
      mic_i u_i ( step );
      
      const mic_m m_u = u_i < grid_u_segments;
      const mic_m m_v = v_i < grid_v_segments;
      
      for (unsigned int x=0;x<grid_u_res;x+=16, u_i += 16)
      {
	const mic_f u = select(m_u, mic_f(u_i) * inv_grid_u_segments, 1.0f);
	const mic_f v = select(m_v, mic_f(v_i) * inv_grid_v_segments, 1.0f);
	ustore16f(&u_array[index + x],u);
	ustore16f(&v_array[index + x],v);	   
      }
    }       
    
    /* stich different tessellation levels in u/v grid */
    stichUVGrid(edge_levels,grid_u_res,grid_v_res,u_array,v_array);
  }
  
  __forceinline void gridUVTessellator16f(const float edge_levels[4],
					  const unsigned int grid_u_res,
					  const unsigned int grid_v_res,
					  float * __restrict__ const u_array,
					  float * __restrict__ const v_array)
  {
    assert( grid_u_res >= 1);
    assert( grid_v_res >= 1);
    assert( edge_levels[0] >= 1.0f );
    assert( edge_levels[1] >= 1.0f );
    assert( edge_levels[2] >= 1.0f );
    assert( edge_levels[3] >= 1.0f );
    
    const mic_i grid_u_segments = mic_i(grid_u_res)-1;
    const mic_i grid_v_segments = mic_i(grid_v_res)-1;
    
    prefetch<PFHINT_L1EX>(u_array);
    prefetch<PFHINT_L1EX>(v_array);
    
    const mic_i identity ( step );
    
    const mic_m m_u = identity < grid_u_segments;
    const mic_m m_v = identity < grid_v_segments;
    
    const mic_f inv_grid_u_segments = rcp(mic_f(grid_u_segments));
    const mic_f inv_grid_v_segments = rcp(mic_f(grid_v_segments));
    
    //const mic_f identity( step );
    
    
    const mic_f u_values = select(m_u, mic_f(identity) * inv_grid_u_segments, 1.0f);
    const mic_f v_values = select(m_v, mic_f(identity) * inv_grid_v_segments, 1.0f);
    
    /* initialize grid */
    unsigned int index = 0;
    for (unsigned int y=0;y<grid_v_res;y++,index+=grid_u_res)
    {
      const mic_f v = v_values[y];
      const mic_f u = u_values;
      ustore16f_low(&u_array[index],u);
      ustore16f_low(&v_array[index],v);
    }       
    
#if 0
    DBG_PRINT("UV grid");
    DBG_PRINT( edge_levels[0] );
    DBG_PRINT( edge_levels[1] );
    DBG_PRINT( edge_levels[2] );
    DBG_PRINT( edge_levels[3] );
    
    DBG_PRINT( grid_u_res );
    DBG_PRINT( grid_v_res );
    
    for (unsigned int y=0;y<grid_v_res;y++)
    {
      std::cout << "row " << y << " ";
      for (unsigned int x=0;x<grid_u_res;x++)
	std::cout << "(" << v_array[grid_u_res*y+x] << "," << u_array[grid_u_res*y+x] << ") ";
      std::cout << std::endl;
    }
#endif
    
  }
  
#endif
}

