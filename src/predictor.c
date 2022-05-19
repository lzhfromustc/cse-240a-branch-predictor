//========================================================//
//  predictor.c                                           //
//  Source file for the Branch Predictor                  //
//                                                        //
//  Implement the various branch predictors below as      //
//  described in the README                               //
//========================================================//
#include <stdio.h>
#include <math.h>
#include "predictor.h"

//
// TODO:Student Information
//
const char *studentName = "Ziheng Liu";
const char *studentID   = "A59010078";
const char *email       = "zil060@ucsd.edu";


// Utilities
int my_pow2(int input) {
  return 1 << input;
}

int my_log2(int input) {
  for (int i = 0; i < 30; i++) {
    int try = 1 << i;
    if (try == input) {
      return i;
    }
  }
  return 0;
}

void print_backward_binary(uint32_t input) {
  while (input) {
    if (input & 1 )
        printf("1");
    else
        printf("0");

    input = input >> 1;
  }
  printf("\n");
}

// Definitions for 2-bit counters
#define SN  0			// predict NT, strong not taken
#define WN  1			// predict NT, weak not taken
#define WT  2			// predict T, weak taken
#define ST  3			// predict T, strong taken

//------------------------------------//
//      Predictor Configuration       //
//------------------------------------//

// Handy Global for use in output routines
const char *bpName[4] = { "Static", "Gshare",
                          "Tournament", "Custom" };

//define number of bits required for indexing the BHT here. 
int ghistoryBits = 14; // Number of bits used for Global History
int bpType;       // Branch Prediction Type
int verbose;


//------------------------------------//
//      Predictor Data Structures     //
//------------------------------------//

//
//TODO: Add your own Branch Predictor data structures here
//
//-------gshare------------
uint8_t *bht_gshare;
uint64_t ghistory;

//-------tournament--------
// setting: 
// global part
// #define TOUR_G_ENTRY 4 * 1024
// uint8_t *tour_g_bht; // a table with TOUR_G_ENTRY entries, and each entry uses 2 bits
// uint64_t tour_g_history; // use only the last log2(TOUR_G_ENTRY) bits
// // local part
// #define TOUR_L_ENTRY 1 * 1024
// #define TOUR_L_HISTORY 10
// uint16_t *tour_l_history; // a table with TOUR_L_ENTRY entries (1K pc), and each entry uses TOUR_L_HISTORY bits for history
// uint8_t *tour_l_pattern; // a table with 2^TOUR_L_HISTORY entries, and each entry uses 2 bits
// // choice part
// #define TOUR_C_ENTRY 4 * 1024
// uint8_t *tour_c_choice; // a table with TOUR_C_ENTRY entries, and each entry uses 2 bits
int tour_historyBits = 12;
int pcIndexBits = 10;
int lhistoryBits = 11;
#define TOUR_G_ENTRY my_pow2(tour_historyBits)
uint8_t *tour_g_bht; // a table with TOUR_G_ENTRY entries, and each entry uses 2 bits
uint64_t tour_g_history; // use only the last log2(TOUR_G_ENTRY) bits
// local part
#define TOUR_L_ENTRY my_pow2(pcIndexBits)
#define TOUR_L_HISTORY lhistoryBits
uint32_t *tour_l_history; // a table with TOUR_L_ENTRY entries (1K pc), and each entry uses TOUR_L_HISTORY bits for history
uint8_t *tour_l_pattern; // a table with 2^TOUR_L_HISTORY entries, and each entry uses 2 bits
// choice part
#define TOUR_C_ENTRY my_pow2(tour_historyBits)
uint8_t *tour_c_choice; // a table with TOUR_C_ENTRY entries, and each entry uses 2 bits


//-------tage--------
// design based on an article https://ssine.ink/en/posts/tage-predictor/

// base predictor part
#define TAGE_BASE_ENTRY 8 * 1024
uint8_t *tage_base_gshare;
uint64_t tage_base_history;

// tagged predictor part
#define TAGE_COMP_NUM 5 // there are 5 components, and each component is a tagged predictor
#define TAGE_TAG_LEN 10
#define TAGE_COMP_ENTRY 256 // number of entries in each component
const uint8_t TAGE_HISTORY_LEN[TAGE_COMP_NUM] = {80, 40, 20, 10, 5}; // the length of history used by each component is different
#define TAGE_G_HISTORY_LEN 128

uint8_t tage_g_history[TAGE_G_HISTORY_LEN]; // a very long history, and each component will use part of it
uint32_t tage_l_history;

typedef struct tage_comp_entry {
  uint16_t tag; // another hash of pc and history
  uint8_t choice; // 2-bit state machine
  uint8_t usage; // whether this entry is useful
} comp_entry;

typedef struct tage_history {
  uint8_t len_history;
  uint64_t compressed;
} history;

typedef struct tage_comp {
  comp_entry entry[TAGE_COMP_ENTRY];
  history index;
  history first_tag;
  history second_tag;
  uint8_t len_history;
} comp;

comp tage_comp_list[TAGE_COMP_NUM];
uint16_t tage_comp_entry_index[TAGE_COMP_NUM]; // store the entry index in tage_comp

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//



// Initialize the predictor
//

//gshare functions
void init_gshare() {
 int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t*)malloc(bht_entries * sizeof(uint8_t));
  // unsigned long size_alloc = 0;
  // size_alloc += bht_entries;
  // printf("gshare predictor malloc used %lu entries, and each entry is 2 bit\n", size_alloc);
  // printf("TEST: log2 of 4*1024 is %d\n", my_log2(4*1024));
  // printf("TEST: 2^12 is %d\n", my_pow2(12));
  int i = 0;
  for(i = 0; i< bht_entries; i++){
    bht_gshare[i] = WN;
  }
  ghistory = 0;
}



uint8_t 
gshare_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch(bht_gshare[index]){
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
      return NOTTAKEN;
  }
}

void
train_gshare(uint32_t pc, uint8_t outcome) {
  //get lower ghistoryBits of pc
  uint32_t bht_entries = 1 << ghistoryBits;
  uint32_t pc_lower_bits = pc & (bht_entries-1);
  uint32_t ghistory_lower_bits = ghistory & (bht_entries -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  //Update state of entry in bht based on outcome
  switch(bht_gshare[index]){
    case WN:
      bht_gshare[index] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      bht_gshare[index] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      bht_gshare[index] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      bht_gshare[index] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in GSHARE BHT!\n");
  }

  //Update history register
  ghistory = ((ghistory << 1) | outcome); 
}

void
cleanup_gshare() {
  free(bht_gshare);
}

// --------Tournament functions

// init

void init_tournament() {
  tour_g_history = 0;
  tour_g_bht = (uint8_t*)malloc(TOUR_G_ENTRY * sizeof(uint8_t));
  int i = 0;
  for(i = 0; i< TOUR_G_ENTRY; i++){
    tour_g_bht[i] = WN;
  }
  // printf("tournament predictor's global pattern table has %d entries, and each entry is 2 bit\n", TOUR_G_ENTRY);

  tour_l_history = (uint32_t*)malloc(TOUR_L_ENTRY * sizeof(uint32_t));
  tour_l_pattern = (uint8_t*)malloc(my_pow2(TOUR_L_HISTORY) * sizeof(uint8_t));
  for(i = 0; i< TOUR_L_ENTRY; i++){
    tour_l_history[i] = 0;
  }
  for(i = 0; i< my_pow2(TOUR_L_HISTORY); i++){
    tour_l_pattern[i] = WN;
  }
  printf("tournament's l_pattern has %d entries\n", my_pow2(TOUR_L_HISTORY));
  for(i = 0; i< my_pow2(TOUR_L_HISTORY); i++){
    printf("%u;", tour_l_pattern[i]);
  }
  printf("\n");
  // printf("tournament predictor's local pattern table has %d entries, and each entry is 2 bit\n", my_pow2(TOUR_L_HISTORY));
  // printf("tournament predictor's local table has %d entries, and each entry is %d bit\n", TOUR_L_ENTRY, TOUR_L_HISTORY);

  tour_c_choice = (uint8_t*)malloc(TOUR_C_ENTRY * sizeof(uint8_t));
  for(i = 0; i< TOUR_C_ENTRY; i++){
    tour_c_choice[i] = WN;
  }
  // printf("tournament predictor's choice pattern table has %d entries, and each entry is 2 bit\n", TOUR_C_ENTRY);
  
}

// predict

uint8_t 
tournament_g_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t pc_lower_bits = pc & (TOUR_G_ENTRY-1);
  uint32_t ghistory_lower_bits = tour_g_history & (TOUR_G_ENTRY -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch(tour_g_bht[index]){
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in Tournament g predict!\n");
      return NOTTAKEN;
  }
}

uint8_t 
tournament_l_predict(uint32_t pc) {
  // get lower bits of pc
  uint32_t pc_lower_bits = pc & (TOUR_L_ENTRY-1);
  // get local history of this branch
  uint32_t local_history = tour_l_history[pc_lower_bits];
  local_history = local_history & (my_pow2(TOUR_L_HISTORY) - 1);
  // if (pc == 4259562) {
  //     print_backward_binary(local_history);
  // }
  if (local_history >= my_pow2(TOUR_L_HISTORY)) {
    printf("Error in l_predict: local_history is %u, but entry num is my_pow2(TOUR_L_HISTORY)\n", local_history);
  }
  
  // get prediction
  switch(tour_l_pattern[local_history]) {
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in Tournament l predict: %u\n", tour_l_pattern[local_history]);
      return NOTTAKEN;
  }
}

uint8_t 
tournament_predict(uint32_t pc) {
  uint8_t g_predict = tournament_g_predict(pc);
  uint8_t l_predict = tournament_l_predict(pc);
  //get lower bits of global_history
  uint32_t g_lower_bits = tour_g_history & (TOUR_C_ENTRY-1);
  switch(tour_c_choice[g_lower_bits]){
    case WN:
      return g_predict;
    case SN:
      return g_predict;
    case WT:
      return l_predict;
    case ST:
      return l_predict;
    default:
      printf("Warning: Undefined state of entry in Tournament predict choice!\n");
      return g_predict;
  }
}

// train functions

void
tournament_g_train(uint32_t pc, uint8_t outcome) {
  //get lower bits of pc
  uint32_t pc_lower_bits = pc & (TOUR_G_ENTRY-1);
  uint32_t ghistory_lower_bits = tour_g_history & (TOUR_G_ENTRY -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  //Update state of entry in bht based on outcome
  switch(tour_g_bht[index]){
    case WN:
      tour_g_bht[index] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      tour_g_bht[index] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      tour_g_bht[index] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      tour_g_bht[index] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in Tournament g train!\n");
  }

  //Update history register
  tour_g_history = ((tour_g_history << 1) | outcome); 
}

void tournament_l_train(uint32_t pc, uint8_t outcome) {
  // get lower bits of pc
  uint32_t pc_lower_bits = pc & (TOUR_L_ENTRY-1);
  // get local history of this branch
  uint32_t local_history = tour_l_history[pc_lower_bits];
  local_history &= (my_pow2(TOUR_L_HISTORY) - 1);
  if (local_history >= my_pow2(TOUR_L_HISTORY)) {
    printf("Error in l_train: local_history is %u, but entry num is my_pow2(TOUR_L_HISTORY)\n", local_history);
  }
  // train
  switch(tour_l_pattern[local_history]) {
    case WN:
      tour_l_pattern[local_history] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      tour_l_pattern[local_history] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      tour_l_pattern[local_history] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      tour_l_pattern[local_history] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in Tournament l train: %u!\n", tour_l_pattern[local_history]);
  }

  // Update local history
  tour_l_history[pc_lower_bits] <<= 1;
  tour_l_history[pc_lower_bits] &= (my_pow2(TOUR_L_HISTORY) - 1);
  tour_l_history[pc_lower_bits] |= outcome;
}

void
tournament_train(uint32_t pc, uint8_t outcome) {
  uint8_t g_predict = tournament_g_predict(pc);
  uint8_t l_predict = tournament_l_predict(pc);
  //get lower bits of global history
  uint32_t g_lower_bits = tour_g_history & (TOUR_C_ENTRY-1);
  if (g_predict != l_predict) {
    switch(tour_c_choice[g_lower_bits]){
      // Note: different logic here. If l_predict is correct, rely more on local prediction
      case WN:
        tour_c_choice[g_lower_bits] = (outcome != l_predict)?SN:WT;
        break;
      case SN:
        tour_c_choice[g_lower_bits] = (outcome != l_predict)?SN:WN;
        break;
      case WT:
        tour_c_choice[g_lower_bits] = (outcome != l_predict)?WN:ST;
        break;
      case ST:
        tour_c_choice[g_lower_bits] = (outcome != l_predict)?WT:ST;
        break;
      default:
        printf("Warning: Undefined state of entry in Tournament train choice!\n");
    }
  }

  tournament_g_train(pc, outcome);
  tournament_l_train(pc, outcome);
}

// cleanup function

void
cleanup_tournament() {
  free(tour_g_bht);
  free(tour_l_history);
  free(tour_l_pattern);
  free(tour_c_choice);
}

//---------End of Tournament


//---------TAGE functions

// utils

// init

void init_tage() {

  // base
  tage_base_history = 0;
  tage_base_gshare = (uint8_t*)malloc(TAGE_BASE_ENTRY * sizeof(uint8_t));
  int i = 0;
  for(i = 0; i< TAGE_BASE_ENTRY; i++) {
    tage_base_gshare[i] = WN;
  }

  // component global
  for(i = 0; i< TAGE_G_HISTORY_LEN; i++) {
    tage_g_history[i] = 0;
  }
  tage_l_history = 0;

  // component each
  for(i = 0; i< TAGE_COMP_NUM; i++) {
    tage_comp_entry_index[i] = 0;

    int j = 0;
    for(j = 0; j< TAGE_COMP_ENTRY; j++) {
      tage_comp_list[i].entry[j].choice = 00;
      tage_comp_list[i].entry[j].tag = 0; // 10 0s 
      tage_comp_list[i].entry[j].usage = 00;
    }

    tage_comp_list[i].first_tag.len_history = TAGE_HISTORY_LEN[i];
    tage_comp_list[i].first_tag.compressed = 0;

    tage_comp_list[i].second_tag.len_history = TAGE_HISTORY_LEN[i];
    tage_comp_list[i].second_tag.compressed = 0;

    tage_comp_list[i].index.len_history = TAGE_HISTORY_LEN[i];
    tage_comp_list[i].index.compressed = 0;

    tage_comp_list[i].len_history = TAGE_HISTORY_LEN[i];
  }

}

// predict

uint8_t 
tage_base_predict(uint32_t pc) {
  uint32_t pc_lower_bits = pc & (TAGE_BASE_ENTRY-1);
  uint32_t ghistory_lower_bits = tage_base_history & (TAGE_BASE_ENTRY -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;
  switch(tage_base_gshare[index]){
    case WN:
      return NOTTAKEN;
    case SN:
      return NOTTAKEN;
    case WT:
      return TAKEN;
    case ST:
      return TAKEN;
    default:
      printf("Warning: Undefined state of entry in TAGE base GSHARE BHT!\n");
      return NOTTAKEN;
  }
}

uint8_t 
tage_predict(uint32_t pc) {
  uint8_t base_predict = tage_base_predict(pc);
  return base_predict;
}



// train
void
train_tage_base(uint32_t pc, uint8_t outcome) {
  //get lower ghistoryBits of pc
  uint32_t pc_lower_bits = pc & (TAGE_BASE_ENTRY-1);
  uint32_t ghistory_lower_bits = tage_base_history & (TAGE_BASE_ENTRY -1);
  uint32_t index = pc_lower_bits ^ ghistory_lower_bits;

  //Update state of entry in bht based on outcome
  switch(tage_base_gshare[index]){
    case WN:
      tage_base_gshare[index] = (outcome==TAKEN)?WT:SN;
      break;
    case SN:
      tage_base_gshare[index] = (outcome==TAKEN)?WN:SN;
      break;
    case WT:
      tage_base_gshare[index] = (outcome==TAKEN)?ST:WN;
      break;
    case ST:
      tage_base_gshare[index] = (outcome==TAKEN)?ST:WT;
      break;
    default:
      printf("Warning: Undefined state of entry in TAGE base GSHARE BHT!\n");
  }

  //Update history register
  tage_base_history = ((tage_base_history << 1) | outcome); 
}

void
tage_train(uint32_t pc, uint8_t outcome) {
  train_tage_base(pc, outcome);
}


//---------End of TAGE

void
init_predictor()
{
  switch (bpType) {
    case STATIC:
    case GSHARE:
      init_gshare();
      break;
    case TOURNAMENT:
      init_tournament();
      break;
    case CUSTOM:
      init_tage();
      break;
    default:
      break;
  }
  
}

// Make a prediction for conditional branch instruction at PC 'pc'
// Returning TAKEN indicates a prediction of taken; returning NOTTAKEN
// indicates a prediction of not taken
//
uint8_t
make_prediction(uint32_t pc)
{

  // Make a prediction based on the bpType
  switch (bpType) {
    case STATIC:
      return TAKEN;
    case GSHARE:
      return gshare_predict(pc);
    case TOURNAMENT:
      return tournament_predict(pc);
    case CUSTOM:
      return tage_predict(pc);
    default:
      break;
  }

  // If there is not a compatable bpType then return NOTTAKEN
  return NOTTAKEN;
}

// Train the predictor the last executed branch at PC 'pc' and with
// outcome 'outcome' (true indicates that the branch was taken, false
// indicates that the branch was not taken)
//

void
train_predictor(uint32_t pc, uint8_t outcome)
{

  switch (bpType) {
    case STATIC:
    case GSHARE:
      return train_gshare(pc, outcome);
    case TOURNAMENT:
      return tournament_train(pc, outcome);
    case CUSTOM:
      return tage_train(pc, outcome);
    default:
      break;
  }
  

}
