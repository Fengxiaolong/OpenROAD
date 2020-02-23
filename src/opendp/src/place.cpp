/////////////////////////////////////////////////////////////////////////////
// Original authors: SangGi Do(sanggido@unist.ac.kr), Mingyu Woo(mwoo@eng.ucsd.edu)
//          (respective Ph.D. advisors: Seokhyeong Kang, Andrew B. Kahng)
// Rewrite by James Cherry, Parallax Software, Inc.
//
// BSD 3-Clause License
//
// Copyright (c) 2019, James Cherry, Parallax Software, Inc.
// Copyright (c) 2018, SangGi Do and Mingyu Woo
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are met:
//
// * Redistributions of source code must retain the above copyright notice, this
//   list of conditions and the following disclaimer.
//
// * Redistributions in binary form must reproduce the above copyright notice,
//   this list of conditions and the following disclaimer in the documentation
//   and/or other materials provided with the distribution.
//
// * Neither the name of the copyright holder nor the names of its
//   contributors may be used to endorse or promote products derived from
//   this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
// IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
// ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
// LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
// POSSIBILITY OF SUCH DAMAGE.
///////////////////////////////////////////////////////////////////////////////

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include "opendp/Opendp.h"

namespace opendp {

using std::abs;
using std::cerr;
using std::cout;
using std::endl;
using std::make_pair;
using std::pair;
using std::sort;
using std::string;
using std::vector;

static bool sortUpOrder(Cell* cell1, Cell* cell2) {
  int area1 = cell1->area();
  int area2 = cell2->area();
  if(area1 > area2)
    return true;
  else if(area1 < area2)
    return false;
  else if(cell1->dense_factor > cell2->dense_factor)
    return true;
  else if(cell1->dense_factor < cell2->dense_factor)
    return false;
  else
    return cell1->db_inst->getId() < cell2->db_inst->getId();
}

void Opendp::simplePlacement(bool verbose) {
  if(groups_.size() > 0) {
    // group_cell -> region assign
    group_cell_region_assign();
    if (verbose)
      cout << "Notice: group instance region assignment done." << endl;
  }
  // non group cell -> sub region gen & assign
  non_group_cell_region_assign();
  if (verbose)
      cout << "Notice: non group instance region assignment done." << endl;

  // pre placement out border ( Need region assign function previously )
  if(groups_.size() > 0) {
    group_cell_pre_placement();
    if (verbose)
      cout << "Notice: group instance pre-placement done." << endl;
    non_group_cell_pre_placement();
    if (verbose)
      cout << "Notice: Non group instance pre-placement done." << endl;
  }

  // naive method placement ( Multi -> single )
  if(groups_.size() > 0) {
    group_cell_placement();
    cout << " group_cell_placement done .. " << endl;
    for(Group &group : groups_) {
      for(int j = 0; j < 3; j++) {
        int count_a = group_refine(&group);
        int count_b = group_annealing(&group);
        if(count_a < 10 || count_b < 100) break;
      }
    }
  }
  non_group_cell_placement();
  if (verbose)
    cout << "Notice: non group instance placement done. " << endl;
}

void Opendp::non_group_cell_pre_placement() {
  for(Cell& cell : cells_) {
    bool inGroup = false;
    adsRect* target;
    pair< int, int > coord;
    if(!cell.inGroup() && !cell.is_placed) {
      for(int j = 0; j < groups_.size(); j++) {
	Group* group = &groups_[j];
	for(adsRect& rect : group->regions) {
	  if(check_overlap(&cell, &rect)) {
	    inGroup = true;
	    target = &rect;
	  }
	}
      }
      if(inGroup) {
	pair< int, int > coord =
          nearest_coord_to_rect_boundary(&cell, target);
	if(map_move(&cell, coord.first, coord.second))
	  cell.hold = true;
      }
    }
  }
}

void Opendp::group_cell_pre_placement() {
  for(Group &group : groups_) {
    for(Cell* cell : group.siblings) {
      if(!(isFixed(cell) || cell->is_placed)) {
	int dist = INT_MAX;
	bool in_group = false;
	adsRect* target;
	for(adsRect& rect : group.regions) {
	  if(check_inside(cell, &rect))
	    in_group = true;
	  int temp_dist = dist_for_rect(cell, &rect);
	  if(temp_dist < dist) {
	    dist = temp_dist;
	    target = &rect;
	  }
	}
	if(!in_group) {
	  pair< int, int > coord =
            nearest_coord_to_rect_boundary(cell, target);
	  if(map_move(cell, coord.first, coord.second)) cell->hold = true;
	}
      }
    }
  }
}

void Opendp::non_group_cell_placement() {
  vector< Cell* > cell_list;
  cell_list.reserve(cells_.size());

  for(Cell& cell : cells_) {
    if(!(isFixed(&cell) || cell.inGroup() || cell.is_placed))
      cell_list.push_back(&cell);
  }
  sort(cell_list.begin(), cell_list.end(), sortUpOrder);

  for(Cell* cell : cell_list) {
    Macro* macro = cell->cell_macro;
    if(macro->isMulti) {
      if(!map_move(cell))
	shift_move(cell);
    }
  }
  for(Cell* cell : cell_list) {
    Macro* macro = cell->cell_macro;
    if(!macro->isMulti) {
      if(!map_move(cell)) {
	shift_move(cell);
      }
    }
  }
}

void Opendp::group_cell_placement() {
  for(Group &group : groups_) {
    bool single_pass = true;
    bool multi_pass = true;
    vector< Cell* > cell_list;
    cell_list.reserve(cells_.size());
    for(Cell* cell : group.siblings) {
      if(!isFixed(cell) && !cell->is_placed) {
	cell_list.push_back(cell);
      }
    }
    sort(cell_list.begin(), cell_list.end(), sortUpOrder);
    // place multi-deck cells on each group region
    for(int j = 0; j < cell_list.size(); j++) {
      Cell* cell = cell_list[j];
      if(!isFixed(cell) && !cell->is_placed) {
	assert(cell->inGroup());
	Macro* macro = cell->cell_macro;
	if(macro->isMulti) {
	  multi_pass = map_move(cell);
	  if(!multi_pass) {
	    cout << "map_move fail" << endl;
	    break;
	  }
	}
      }
    }
    // cout << "Group util : " << group->util << endl;
    if(multi_pass) {
      //				cout << " Group : " << group->name <<
      //" multi-deck placement done - ";
      // place single-deck cells on each group region
      for(int j = 0; j < cell_list.size(); j++) {
        Cell* cell = cell_list[j];
        if(!isFixed(cell) && !cell->is_placed) {
	  assert(cell->inGroup());
	  Macro* macro = cell->cell_macro;
	  if(!macro->isMulti) {
	    single_pass = map_move(cell);
	    if(!single_pass) {
	      cout << "map_move fail (single cell)" << endl;
	      break;
	    }
	  }
	}
      }
    }

    if(!single_pass || !multi_pass) {
      // Erase group cells
      for(int j = 0; j < group.siblings.size(); j++) {
        Cell* cell = group.siblings[j];
        erase_pixel(cell);
      }

      // determine brick placement by utilization
      if(group.util > 0.95) {
        brick_placement_1(&group);
      }
      else {
        brick_placement_2(&group);
      }
    }
  }
}

void
Opendp::rectDist(Cell *cell,
		 adsRect *rect,
		 // Return values.
		 int x_tar,
		 int y_tar)
{
  x_tar = 0;
  y_tar = 0;
  int init_x, init_y;
  initLocation(cell, init_x, init_y);
  if(init_x > (rect->xMin() + rect->xMax()) / 2)
    x_tar = rect->xMax();
  else
    x_tar = rect->xMin();
  if(init_y > (rect->yMin() + rect->yMax()) / 2)
    y_tar = rect->yMax();
  else
    y_tar = rect->yMin();
}

int
Opendp::rectDist(Cell *cell,
		 adsRect *rect)
{
  int x_tar, y_tar;
  rectDist(cell, rect,
	   x_tar, y_tar);
  int init_x, init_y;
  initLocation(cell, init_x, init_y);
  return abs(init_x - x_tar) + abs(init_y - y_tar);
}

// place toward group edges
void Opendp::brick_placement_1(Group* group) {
  adsRect *boundary = &group->boundary;
  vector< Cell* > sort_by_dist(group->siblings);

  sort(sort_by_dist.begin(), sort_by_dist.end(),
       [&](Cell* cell1, Cell* cell2) {
         return rectDist(cell1, boundary) < rectDist(cell2, boundary);
       });
			       
  for(Cell* cell : sort_by_dist) {
    int x_tar, y_tar;
    rectDist(cell, boundary);

    bool valid = map_move(cell, x_tar, y_tar);
    if(!valid) {
      cout << "Warning: cannot place single ( brick place 1 ) "
           << cell->name() << endl;
    }
  }
}

// place toward region edges
void Opendp::brick_placement_2(Group* group) {
  vector< Cell* > sort_by_dist(group->siblings);

  sort(sort_by_dist.begin(), sort_by_dist.end(),
       [&](Cell* cell1, Cell* cell2) {
         return rectDist(cell1, cell1->region) < rectDist(cell2, cell2->region);
       });

  for(Cell* cell : sort_by_dist) {
    if(!cell->hold) {
      adsRect *region = cell->region;
      int x_tar, y_tar;
      rectDist(cell, region,
	       x_tar, y_tar);
      bool valid = map_move(cell, x_tar, y_tar);
      if(!valid) {
	cout << "Warning: cannot place single ( brick place 2 ) "
	     << cell->name() << endl;
      }
    }
  }
}

int Opendp::group_refine(Group* group) {
  vector< Cell* > sort_by_disp(group->siblings);

  sort(sort_by_disp.begin(), sort_by_disp.end(),
       [&](Cell* cell1, Cell* cell2) {
         return (disp(cell1) > disp(cell1));
       });

  int count = 0;
  for(int i = 0; i < sort_by_disp.size() * group_refine_percent_; i++) {
    Cell* cell = sort_by_disp[i];
    if(!cell->hold) {
      if(refine_move(cell)) count++;
    }
  }
  // cout << " Group refine : " << count << endl;
  return count;
}

int Opendp::group_annealing(Group* group) {
  srand(777);
  // srand(time(nullptr));
  int count = 0;

  for(int i = 0; i < 1000 * group->siblings.size(); i++) {
    Cell* cellA = group->siblings[rand() % group->siblings.size()];
    Cell* cellB = group->siblings[rand() % group->siblings.size()];

    if(!cellA->hold && !cellB->hold) {
      if(swap_cell(cellA, cellB)) count++;
    }
  }
  // cout << " swap cell count : " << count << endl;
  return count;
}

int Opendp::non_group_annealing() {
  srand(777);
  int count = 0;
  for(int i = 0; i < 100 * cells_.size(); i++) {
    Cell* cellA = &cells_[rand() % cells_.size()];
    Cell* cellB = &cells_[rand() % cells_.size()];
    if(!cellA->hold && !cellB->hold) {
      if(swap_cell(cellA, cellB)) count++;
    }
  }
  // cout << " swap cell count : " << count << endl;
  return count;
}

int Opendp::non_group_refine() {
  vector< Cell* > sort_by_disp;
  sort_by_disp.reserve(cells_.size());

  for(Cell &cell : cells_) {
    if(!(isFixed(&cell) || cell.hold || cell.inGroup()))
      sort_by_disp.push_back(&cell);
  }
  sort(sort_by_disp.begin(), sort_by_disp.end(),
       [&](Cell* cell1, Cell* cell2) {
         return disp(cell1) > disp(cell2);
       });

  int count = 0;
  for(int i = 0; i < sort_by_disp.size() * non_group_refine_percent_; i++) {
    Cell* cell = sort_by_disp[i];
    if(!cell->hold) {
      if(refine_move(cell)) count++;
    }
  }
  // cout << " nonGroup refine : " << count << endl;
  return count;
}

} // namespace opendp
