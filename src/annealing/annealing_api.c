#include <stdlib.h>
#include <assert.h>
#include <stdio.h>

#include "building/construction.h"
#include "building/construction_routed.h"
#include "building/construction_building.h"
#include "building/type.h"
#include "building/building.h"
#include "building/model.h"
#include "map/property.h"
#include "map/point.h"
#include "gsl_siman.h"
#include "annealing/annealing_api.h"
#include "widget/city.h"

void api_place_house(int x, int y) {
    // Place a house at the tile given by x and y
    int placed = place_houses(0, x, y, x, y);
    assert(placed == 1);
}

void api_place_road(int x, int y) {
    // Place a road at the tile given by x and y
    building_construction_set_type(BUILDING_ROAD);
    map_tile tile;
    tile.x = x;
    tile.y = y;
    build_start(&tile);
    build_move(&tile);
    build_end();
}


//void api_place_engineer(int x, int y){
//    int placed = building_construction_place_building(BUILDING_ENGINEERS_POST, x, y);
//    assert(placed == 1);
//}

void api_place_prefecture(int x, int y) {
    // Place a prefecture at the tile given by x and y
    int placed = building_construction_place_building(BUILDING_PREFECTURE, x, y);
    assert(placed == 1);

}

void api_place_nothing(int x, int y) {
    // By not placing anything, we leave empty grass
    // ToDo We should probably delete what was at x and y
    // pass
}

void api_place_well(int x, int y) {
    // Place a well at the tile given by x and y
    int placed = building_construction_place_building(BUILDING_WELL, x, y);
    assert(placed == 1);
}

void api_place_garden(int x, int y) {
    //    int placed = place_garden(x, y, x, y); //building_construction_place_building(BUILDING_GARDENS, x, y);
    //    assert(placed == 1);
    //    data.start.x = x;
    //    data.start.y = y;
    //    data.end.x = x;
    //    data.end.y = y;
    //    data.type = BUILDING_GARDENS;
    //    building_construction_place();
    building_construction_set_type(BUILDING_GARDENS);
    map_tile tile;
    tile.x = x;
    tile.y = y;
    build_start(&tile);
    build_move(&tile);
    build_end();
}

void api_place_market(int x, int y) {
    // Place a market at the tile given by x and y
    building_construction_set_type(BUILDING_MARKET);
    map_tile tile;
    tile.x = x;
    tile.y = y;
    build_start(&tile);
    build_move(&tile);
    build_end();
}


typedef void (*place_building_func)(int, int);
place_building_func place_building_funcs[7] = {
    // These are our choice of buildings
    &api_place_nothing,
    &api_place_house,
    &api_place_road,
    &api_place_prefecture,
    &api_place_garden,
    &api_place_well,
    &api_place_market
    //    &api_place_engineer, 
};


char* place_building_names[7] = {
    "empty land",
    "house",
    "road",
    "prefecture",
    "garden",
    "well",
    "market"
    //    "engineer's post",
};


int place_building_sizes[7] = {
    // These must be sorted in ascending size
    // and are the size in one dimension (i.e. width not area)
    1,
    1,
    1,
    1,
    1,
    1,
    2
    //    1
};

unsigned int global_building_uid_counter = 1;

char* api_get_building_name(int i) {
    return place_building_names[i];
}

int api_score_city() {
    // The same as ratings.calculate_max_prosperity but we take the total
    // prosperity, not the average per house

    int points = 0;
    for (int i = 1; i < MAX_BUILDINGS; i++) {
        building *b = building_get(i);
        if (b->state && b->house_size) {
            points += model_get_house(b->subtype.house_level)->prosperity;
        }
    }
    return points;
}

int already_built(unsigned int* built_uids, unsigned int max, unsigned int uid) {
    // does c have bool?
    for (int i = 0; i < max; i++) {
        if (built_uids[i] == uid) {
            return 1;
        }
    }
    return 0;
}

void api_build_buildings(void* xp) {
    map_property_clear_constructing_and_deleted();
    ab(*squares)[ANNEAL_DIM] = (ab(*)[ANNEAL_DIM])xp;

    unsigned int built_uids[ANNEAL_DIM * ANNEAL_DIM];
    unsigned int uid_index = 0;

    for (int x = 0; x < ANNEAL_DIM; x++) {
        for (int y = 0; y < ANNEAL_DIM; y++) {
            if (already_built(built_uids, uid_index, squares[x][y].uid)) {
                continue;
            } else {
                ab my_building = squares[x][y];
                place_building_funcs[my_building.building_type](x + 7, y + 7);
                built_uids[uid_index] = my_building.uid;
                uid_index++;
            }
        }
    }
}

void api_modify_elements_r(void* xp, int num_elements, int (*rand_a)(void), int (*rand_b)(void)) {
    // Modify up to num_elements of xp, in place
    // Decide on the square to change with rand_a and the new building with rand_b
//    ab(*squares)[ANNEAL_DIM] = (ab(*)[ANNEAL_DIM])xp;

    for (int i = 0; i < num_elements; i++) {
        int square_index = rand_a() % (ANNEAL_DIM * ANNEAL_DIM);
        int x = square_index / ANNEAL_DIM;
        int y = square_index % ANNEAL_DIM;

        //todo
        //        int biggest_buildable_building = 

        int number_of_buildings = sizeof (place_building_funcs) / sizeof (place_building_funcs[0]);
        int new_building_type = rand_b() % number_of_buildings;
        // this causes problems, presumably because there are other copies of this
        // xp made by gsl
        //        free(squares[x][y]);  
//        squares[x][y].building_type = new_building_type;
//        squares[x][y].uid = global_building_uid_counter;
//        global_building_uid_counter++;
        api_replace_building(xp, x, y, new_building_type);
    }
    return;
}

void api_replace_building(void* xp, int x, int y, int new_building_type) {
    // replace the building at xp[x][y] with new_building
    // paving over any buildings that get in the way
    ab(*squares)[ANNEAL_DIM] = (ab(*)[ANNEAL_DIM])xp;

    // firstly, pave over anything that would be touched by the building
    int new_building_size = place_building_sizes[new_building_type];

    for (int i = 0; i < new_building_size; i++) {
        for (int j = 0; j < new_building_size; j++) {
            api_pave_over(squares, x + i, y + j);
        }
    }

    // secondly, place the new building down
    for (int i = 0; i < new_building_size; i++) {
        for (int j = 0; j < new_building_size; j++) {
            squares[x + i][y + j].building_type = new_building_type;
            squares[x + i][y + j].uid = global_building_uid_counter;
        }
    }
    global_building_uid_counter++;
}

void api_pave_over(void* xp, int x, int y) {
    // pave over the square at xp[x][y] and any other tiles that
    // that building covers
    ab(*squares)[ANNEAL_DIM] = (ab(*)[ANNEAL_DIM])xp;

    unsigned int uid = squares[x][y].uid;
    unsigned int building_type = squares[x][y].building_type;
    
    for (int i = 0; i < ANNEAL_DIM; i++) {
        for (int j = 0; j < ANNEAL_DIM; j++) {
            if (squares[i][j].uid == uid) {
                assert(squares[i][j].building_type == building_type);
                
//                if (squares[i][j].building_type != building_type){
//                    printf("x:%d  y:%d  i:%d  j:%d uid:%d\n", x, y, i, j, uid);
//
//
//                    for (int y = 0; y < 4; y++) {
//                        printf("%d %d %d %d\n", squares[0][y].building_type, squares[1][y].building_type, squares[2][y].building_type, squares[3][y].building_type);
//                    }
//
//                    for (int y = 0; y < 4; y++) {
//                        printf("%d %d %d %d\n", squares[0][y].uid, squares[1][y].uid, squares[2][y].uid, squares[3][y].uid);
//                    }
//                    // for debugging
//                    assert(squares[x][y].building_type == squares[i][j].building_type);
//
//                }
                squares[i][j].building_type = 2; // road
                squares[i][j].uid = global_building_uid_counter;
                global_building_uid_counter++;
            }
        }
    }
}

void api_modify_elements(void* xp, int num_elements) {
    // Modify up to num_elements of xp, in place,
    // specifying which random number generators to use
    api_modify_elements_r(xp, num_elements, rand, rand);
}
