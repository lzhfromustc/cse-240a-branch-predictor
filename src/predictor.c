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
int TOUR_G_ENTRY = 4 * 1024;
uint8_t *tour_g_bht; // a table with TOUR_G_ENTRY entries, and each entry uses 2 bits
uint64_t tour_g_history; // use only the last log2(TOUR_G_ENTRY) bits
// local part
int TOUR_L_ENTRY = 1024;
int TOUR_L_HISTORY = 11;
uint16_t *tour_l_history; // a table with TOUR_L_ENTRY entries (1K pc), and each entry uses TOUR_L_HISTORY bits for history
uint8_t *tour_l_pattern; // a table with 2^TOUR_L_HISTORY entries, and each entry uses 2 bits
// choice part
int TOUR_C_ENTRY = 4 * 1024;
uint8_t *tour_c_choice; // a table with TOUR_C_ENTRY entries, and each entry uses 2 bits

//------------------------------------//
//        Predictor Functions         //
//------------------------------------//

// Utilities
int my_pow2(int input) {
  return 1 << input;
}

int my_log2(int input) {
  return (int)log2(input);
}

// Initialize the predictor
//

//gshare functions
void init_gshare() {
 int bht_entries = 1 << ghistoryBits;
  bht_gshare = (uint8_t*)malloc(bht_entries * sizeof(uint8_t));
  unsigned long size_alloc = 0;
  size_alloc += bht_entries;
  printf("gshare predictor malloc used %lu entries, and each entry is 2 bit\n", size_alloc);
  printf("TEST: log2 of 4*1024 is %d\n", my_log2(4*1024));
  printf("TEST: 2^12 is %d\n", my_pow2(12));
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
  printf("tournament predictor's global pattern table has %d entries, and each entry is 2 bit\n", TOUR_G_ENTRY);

  tour_l_history = (uint16_t*)malloc(TOUR_L_ENTRY & sizeof(uint16_t));
  tour_l_pattern = (uint8_t*)malloc(my_pow2(TOUR_L_HISTORY) * sizeof(uint8_t));
  for(i = 0; i< TOUR_L_ENTRY; i++){
    tour_l_history[i] = (uint16_t) 0;
  }
  for(i = 0; i< my_pow2(TOUR_L_HISTORY); i++){
    tour_l_pattern[i] = WN;
  }
  printf("tournament predictor's local pattern table has %d entries, and each entry is 2 bit\n", my_pow2(TOUR_L_HISTORY));
  printf("tournament predictor's local table has %d entries, and each entry is %d bit\n", TOUR_L_ENTRY, TOUR_L_HISTORY);

  tour_c_choice = (uint8_t*)malloc(TOUR_C_ENTRY * sizeof(uint8_t));
  for(i = 0; i< TOUR_C_ENTRY; i++){
    tour_c_choice[i] = WN;
  }
  printf("tournament predictor's choice pattern table has %d entries, and each entry is 2 bit\n", TOUR_C_ENTRY);
  
}

// predict

uint8_t 
tournament_g_predict(uint32_t pc) {
  //get lower ghistoryBits of pc
  uint32_t pc_lower_bits = pc & (TOUR_G_ENTRY-1);
  uint32_t ghistory_lower_bits = ghistory & (TOUR_G_ENTRY -1);
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
  uint16_t local_history = tour_l_history[pc_lower_bits];
  local_history = local_history & (my_pow2(TOUR_L_HISTORY) - 1);
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
      printf("Warning: Undefined state of entry in Tournament l predict!\n");
      return NOTTAKEN;
  }
}

uint8_t 
tournament_predict(uint32_t pc) {
  //TODO
  uint8_t g_predict = tournament_g_predict(pc);
  uint8_t l_predict = tournament_l_predict(pc);
  //get lower bits of pc
  uint32_t pc_lower_bits = pc & (TOUR_C_ENTRY-1);
  switch(tour_c_choice[pc_lower_bits]){
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
  uint32_t ghistory_lower_bits = ghistory & (TOUR_G_ENTRY -1);
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
  uint16_t local_history = tour_l_history[pc_lower_bits];
  local_history = local_history & (my_pow2(TOUR_L_HISTORY) - 1);
  // get prediction
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
      printf("Warning: Undefined state of entry in Tournament l train!\n");
  }

  // Update local history
  tour_l_history[pc_lower_bits] = ((local_history << 1) | outcome); 
}

void
tournament_train(uint32_t pc, uint8_t outcome) {
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
    default:
      break;
  }
  

}
