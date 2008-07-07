// ==========================================================================
// Dedmonwakeen's Raid DPS/TPS Simulator.
// Send questions to natehieter@gmail.com
// ==========================================================================

#include <stdarg.h>
#include <float.h>
#include <time.h>
#include <string>
#include <queue>
#include <vector>
#include <map>
#include <algorithm>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// Patch Specific Modeling ==================================================

struct patch_t
{
   uint64_t mask;
   uint64_t encode( int arch, int version, int revision ) { return arch*10000 + version*100 + revision; }
   patch_t        ( int arch, int version, int revision ) {         mask = encode( arch, version, revision ); }
   void set       ( int arch, int version, int revision ) {         mask = encode( arch, version, revision ); }
   bool before    ( int arch, int version, int revision ) { return mask <  encode( arch, version, revision ); }
   bool after     ( int arch, int version, int revision ) { return mask >= encode( arch, version, revision ); }
   patch_t() { mask = encode( 2, 4, 0 ); }
};

// Forward Declarations ======================================================

struct action_t;
struct attack_t;
struct druid_t;
struct event_t;
struct mage_t;
struct option_t;
struct pet_t;
struct player_t;
struct priest_t;
struct report_t;
struct shaman_t;
struct sim_t;
struct rating_t;
struct stats_t;
struct spell_t;
struct talent_translation_t;
struct target_t;
struct uptime_t;
struct warlock_t;
struct warrior_t;
struct weapon_t;

// Event =====================================================================

struct event_t
{
  event_t*  next;
  sim_t*    sim;
  player_t* player;
  uint32_t  id;
  double    time;
  double    reschedule_time;
  int8_t    invalid;
  const char* name;
  event_t( sim_t* s, player_t* p=0, const char* n=0 ) : 
    next(0), sim(s), player(p), reschedule_time(0), invalid(false), name(n) 
  {
    if( ! name ) name = "unknown";
  }
  void reschedule( double new_time );
  double occurs() { return reschedule_time != 0 ? reschedule_time : time; }
  virtual void execute() { assert(0); }
  virtual ~event_t() {}
};

struct event_compare_t 
{
  bool operator () (event_t* lhs, event_t* rhs ) const
  {
    return( lhs -> time == rhs -> time ) ? ( lhs -> id > rhs -> id ) : ( lhs -> time > rhs -> time );
  }
};

// Simulation Engine =========================================================

enum player_type { DRUID=0, MAGE, PALADIN, PRIEST, SHAMAN, WARLOCK, PLAYER_PET, PLAYER_TYPE_MAX };

enum dmg_type { DMG_DIRECT=0, DMG_OVER_TIME=1 };

enum resource_type { RESOURCE_NONE=0, RESOURCE_HEALTH, RESOURCE_MANA, RESOURCE_RAGE, RESOURCE_ENERGY, RESOURCE_FOCUS, RESOURCE_RUNIC, RESOURCE_MAX };

enum result_type { RESULT_NONE=0, RESULT_MISS, RESULT_RESIST, RESULT_DODGE, RESULT_PARRY, RESULT_BLOCK, RESULT_GLANCE, RESULT_CRUSH, RESULT_CRIT, RESULT_HIT, RESULT_MAX };

enum action_type { ACTION_OTHER=0, ACTION_SPELL, ACTION_ATTACK, ACTION_MAX };

enum school_type {
     SCHOOL_NONE=0, 
     SCHOOL_HOLY, 
     SCHOOL_SHADOW, 
     SCHOOL_ARCANE, 
     SCHOOL_FROST, 
     SCHOOL_FIRE, 
     SCHOOL_NATURE, 
     SCHOOL_PHYSICAL,
     SCHOOL_MAX 
};

enum talent_tree_type {
  TREE_NONE=0,
  TREE_BALANCE,    TREE_FERAL,       TREE_RESTORATION, // DRUID
  TREE_ARCANE,     TREE_FIRE,        TREE_FROST,       // MAGE
  TREE_DISCIPLINE, TREE_HOLY,        TREE_SHADOW,      // PRIEST
  TREE_ELEMENTAL,  TREE_ENHANCEMENT,                   // SHAMAN
  TREE_AFFLICTION, TREE_DEMONOLOGY,  TREE_DESTRUCTION, // WARLOCK
  TREE_ARMS,       TREE_PROTECTION,  TREE_FURY,        // WARRIOR
  TALENT_TREE_MAX
};

enum weapon_type { WEAPON_NONE=0, 
		   WEAPON_DAGGER,   WEAPON_FIST,                                                                    WEAPON_SMALL,
		   WEAPON_BEAST_1H, WEAPON_SWORD,    WEAPON_MACE,     WEAPON_AXE,                                   WEAPON_1H,
		   WEAPON_BEAST_2H, WEAPON_SWORD_2H, WEAPON_MACE_2H,  WEAPON_AXE_2H, WEAPON_STAFF,  WEAPON_POLEARM, WEAPON_2H,
		   WEAPON_BOW,      WEAPON_CROSSBOW, WEAPON_GUN,      WEAPON_WAND,   WEAPON_THROWN, WEAPON_RANGED,
		   WEAPON_MAX };

enum position_type { POSITION_NONE=0, POSITION_FRONT, POSITION_BACK, POSITION_MAX };

enum sim_method { SIM_LIST=0, SIM_WHEEL, SIM_PRIORITYQ };

typedef std::priority_queue< event_t*, std::deque<event_t*>, event_compare_t > timing_priorityq_t;

struct timing_wheel_t
{
  double    time_horizon;
  double    granularity;
  int32_t   size;
  event_t** events;
  timing_wheel_t();
  void     push( event_t* );
  event_t* top();
  void     pop();
  bool     empty();
  void     clear();
};

struct timing_list_t
{
  event_t* events;
  timing_list_t();
  void     push( event_t* );
  event_t* top();
  void     pop();
  bool     empty();
  void     clear();
};

struct sim_t
{
  // Only one of these simulation methods in use.
  timing_priorityq_t timing_priorityq;
  timing_wheel_t     timing_wheel;
  timing_list_t      timing_list;
  std::string        method_str;
  int8_t             method;

  // Common Core 
  std::string patch_str;
  patch_t     patch;
  player_t*   player_list;
  target_t*   target;
  double      current_time, max_time, lag, total_seconds;
  int32_t     events_remaining, max_events_remaining;
  int32_t     events_processed, total_events_processed;
  int32_t     seed, id, iterations;
  int8_t      infinite_resource[ RESOURCE_MAX ];
  int8_t      average_dmg, log, debug, timestamp;
  report_t*   report;
  uptime_t*   uptime_list;

  sim_t();

  void      add_event( event_t* );
  void      reschedule_event( event_t* );
  void      flush_events();
  event_t*  next_event();
  bool      execute();
  void      reset();
  bool      init();
  uptime_t* get_uptime( const std::string& name );
  bool      parse_option( const std::string& name, const std::string& value );
  void      print_options();
};

// Gear Rating Conversions ===================================================

struct rating_t
{
  double haste;
  double spell_hit, spell_crit;
  double attack_hit, attack_crit, attack_expertise;
  rating_t() { memset( this, 0x00, sizeof(rating_t) ); }
  void init( int level );
};

// Player ====================================================================

typedef std::map<std::string,int32_t> proc_list_t;
typedef std::map<std::string,double>  gain_list_t;

struct player_t
{
  sim_t*      sim;
  std::string name_str;
  player_t*   next;   
  int8_t      type, level, party;
  double      gcd_ready, base_gcd;
  int8_t      sleeping;
  rating_t    rating;
  pet_t*      pet_list;

  // Haste
  int16_t base_haste_rating, initial_haste_rating, haste_rating;
  double  haste;

  // Core Stats
  int16_t     base_strength,  initial_strength,  strength;
  int16_t     base_agility,   initial_agility,   agility;
  int16_t     base_stamina,   initial_stamina,   stamina;
  int16_t     base_intellect, initial_intellect, intellect;
  int16_t     base_spirit,    initial_spirit,    spirit;

  // Spell Mechanics
  double      base_spell_power,       initial_spell_power[ SCHOOL_MAX+1 ], spell_power[ SCHOOL_MAX+1 ];
  double      base_spell_hit,         initial_spell_hit,                   spell_hit;
  double      base_spell_crit,        initial_spell_crit,                  spell_crit;
  int16_t     base_spell_penetration, initial_spell_penetration,           spell_penetration;
  int16_t     base_mp5,               initial_mp5,                         mp5;
  double      spell_crit_per_intellect;
  double      last_cast;

  // Attack Mechanics
  double      base_attack_power,       initial_attack_power,        attack_power;
  double      base_attack_hit,         initial_attack_hit,          attack_hit;
  double      base_attack_expertise,   initial_attack_expertise,    attack_expertise;
  double      base_attack_crit,        initial_attack_crit,         attack_crit;
  int16_t     base_attack_penetration, initial_attack_penetration,  attack_penetration;
  double      attack_power_per_strength;
  double      attack_power_per_agility;
  double      attack_crit_per_agility;
  weapon_t*   main_hand_weapon;
  weapon_t*   off_hand_weapon;
  weapon_t*   ranged_weapon;
  int8_t      position;

  // Resources
  int16_t resource_base   [ RESOURCE_MAX ];
  double  resource_initial[ RESOURCE_MAX ];
  double  resource_current[ RESOURCE_MAX ];
  int8_t  resource_constrained;
  int16_t resource_constrained_count;
  double  resource_constrained_total_dmg;
  double  resource_constrained_total_time;
  double  mana_per_intellect;
  double  health_per_stamina;

  // Events
  event_t* executing;
  event_t* channeling;

  // Action Priority List
  action_t*   action_list;
  std::string action_list_prefix;
  std::string action_list_str;
  std::string action_list_postfix;
  std::string action_list_skip;

  // Reporting
  int8_t       quiet;
  report_t*    report;
  double       iteration_dmg;
  double       resource_lost  [ RESOURCE_MAX ];
  double       resource_gained[ RESOURCE_MAX ];
  proc_list_t  proc_list;
  gain_list_t  gain_list;
  stats_t*    stats_list;

  struct gear_t
  {
    // Haste
    int16_t haste_rating, haste_rating_enchant;
    // Core Stats
    int16_t strength,  strength_enchant;
    int16_t agility,   agility_enchant;
    int16_t stamina,   stamina_enchant;
    int16_t intellect, intellect_enchant;
    int16_t spirit,    spirit_enchant;
    // Spell Gear
    int16_t spell_power[ SCHOOL_MAX+1 ], spell_power_enchant;
    int16_t spell_hit_rating,            spell_hit_rating_enchant;
    int16_t spell_crit_rating,           spell_crit_rating_enchant;
    int16_t spell_penetration,           spell_penetration_enchant;
    int16_t mp5, mp5_enchant;
    // Attack Gear
    int16_t attack_power,            attack_power_enchant;
    int16_t attack_expertise_rating, attack_expertise_rating_enchant;
    int16_t attack_hit_rating,       attack_hit_rating_enchant;
    int16_t attack_crit_rating,      attack_crit_rating_enchant;
    int16_t attack_penetration,      attack_penetration_enchant;
    // Resource Gear
    int16_t resource        [ RESOURCE_MAX ];
    int16_t resource_enchant[ RESOURCE_MAX ];
    // Budgeting
    int16_t spell_power_budget;
    int16_t attack_power_budget;
    int8_t  budget_slots;
    // Unique Gear
    int8_t  ashtongue_talisman;
    int8_t  chaotic_skyfire;
    int8_t  darkmoon_crusade;
    int8_t  darkmoon_wrath;
    int8_t  elder_scribes;
    int8_t  eternal_sage;
    int8_t  eye_of_magtheridon;
    int8_t  lightning_capacitor;
    int8_t  mark_of_defiance;
    int8_t  mystical_skyfire;
    int8_t  quagmirrans_eye;
    int8_t  sextant_of_unstable_currents;
    int8_t  shiffars_nexus_horn;
    int8_t  spellstrike;
    int8_t  spellsurge;
    int8_t  talisman_of_ascendance;
    int8_t  timbals_crystal;
    int8_t  wrath_of_cenarius;
    int8_t  zandalarian_hero_charm;
    int8_t  tier4_2pc, tier4_4pc;
    int8_t  tier5_2pc, tier5_4pc;
    int8_t  tier6_2pc, tier6_4pc;
    
    gear_t() { memset( (void*) this, 0x00, sizeof( gear_t ) ); }

    void allocate_spell_power_budget( sim_t* );
    void allocate_attack_power_budget( sim_t* );
  };
  gear_t gear;
  
  struct consumable_t
  {
    int8_t arcane_elixir;
    int8_t brilliant_mana_oil;
    int8_t brilliant_wizard_oil;
    int8_t elixir_of_major_shadow_power;
    int8_t elixir_of_shadow_power;
    int8_t flask_of_supreme_power;
    int8_t greater_arcane_elixir;
    int8_t mageblood_potion;
    int8_t nightfin_soup;
    
    consumable_t() { memset( (void*) this, 0x00, sizeof( consumable_t ) ); }
  };
  consumable_t consumables;
  
  struct buff_t
  {
    // Permanent Buffs
    int8_t blessing_of_salvation;
    int8_t blessing_of_wisdom;
    int8_t improved_divine_spirit;
    int8_t fel_armor;
    int8_t mage_armor;
    int8_t molten_armor;
    int8_t sacrifice_pet;
    int8_t sanctity_aura;
    int8_t shadow_form; 
    // Temporary Buffs
    int8_t  temporary_buffs;
    int8_t  amplify_curse;
    int8_t  arcane_blast;
    int8_t  arcane_potency;
    int8_t  arcane_power;
    int8_t  bloodlust;
    double  cast_time_reduction;
    int8_t  clear_casting;
    int8_t  combustion;
    int8_t  combustion_crits;
    int8_t  darkmoon_crusade;
    int8_t  darkmoon_wrath;
    int8_t  evocation;
    int8_t  icy_veins;
    int8_t  inner_focus;
    int8_t  innervate;
    int8_t  lightning_capacitor;
    double  mana_cost_reduction;
    double  moonkin_aura;
    int8_t  nightfall;
    int8_t  natures_grace;
    int8_t  power_infusion;
    int8_t  surge_of_light;
    int16_t talisman_of_ascendance;
    int8_t  totem_of_wrath;
    int8_t  vampiric_embrace;
    int8_t  vampiric_touch;
    int8_t  violet_eye;
    int8_t  wrath_of_air;
    int16_t zandalarian_hero_charm;
    int8_t  tier4_2pc, tier4_4pc;
    int8_t  tier5_2pc, tier5_4pc;
    int8_t  tier6_2pc, tier6_4pc;
    buff_t() { memset( (void*) this, 0x0, sizeof( buff_t ) ); }
    void reset()
    { 
      size_t delta = ( (uintptr_t) &temporary_buffs ) - ( (uintptr_t) this );
      memset( (void*) &temporary_buffs, 0x0, sizeof( buff_t ) - delta );
    }
  };
  buff_t buffs;
  
  struct expirations_t
  {
    float spellsurge;
    event_t* arcane_blast;
    event_t* ashtongue_talisman;
    event_t* darkmoon_crusade;
    event_t* darkmoon_wrath;
    event_t* elder_scribes;
    event_t* eternal_sage;
    event_t* eye_of_magtheridon;
    event_t* lightning_capacitor;
    event_t* mark_of_defiance;
    event_t* mystical_skyfire;
    event_t* mystical_skyfire_silent_cooldown;
    event_t* quagmirrans_eye;
    event_t* sextant_of_unstable_currents;
    event_t* shiffars_nexus_horn;
    event_t* spellstrike;
    event_t* timbals_crystal;
    event_t* wrath_of_cenarius;
    event_t *tier4_2pc, *tier4_4pc;
    event_t *tier5_2pc, *tier5_4pc;
    event_t *tier6_2pc, *tier6_4pc;
    void reset() { memset( (void*) this, 0x00, sizeof( expirations_t ) ); }
    expirations_t() { reset(); }
  };
  expirations_t expirations;
  
  struct enchants_t
  {
    int8_t place_holder;
    void reset() { memset( (void*) this, 0x00, sizeof( enchants_t ) ); }
    enchants_t() { reset(); }
  };
  enchants_t enchants;

  player_t( sim_t* sim, int8_t type, const std::string& name );
  
  virtual const char* name() { return name_str.c_str(); }

  virtual void init();
  virtual void init_base() = 0;
  virtual void init_core();
  virtual void init_spell();
  virtual void init_attack();
  virtual void init_resources();
  virtual void init_actions();
  virtual void init_rating();
  virtual void init_stats();
  virtual void reset();

  virtual double    gcd(); 
  virtual void      schedule_ready( double delta_time=0 );
  virtual action_t* execute_action();

  virtual void regen() {}
  virtual void resource_gain( int8_t resource, double amount, const char* source=0 );
  virtual void resource_loss( int8_t resource, double amount, const char* source=0 );
  virtual bool resource_available( int8_t resource, double cost );
  virtual void check_resources();

  virtual void  summon_pet( const std::string& name );
  virtual void dismiss_pet( const std::string& name );

  // Managing action_xxx events:
  // (1) To "throw" an event, ALWAYS invoke the action_xxx function.
  // (2) To "catch" an event, ALWAYS implement a spell_xxx or attack_xxx virtual function in player sub-class.
  // Disregarding these instructions may result in serious injury and/or death.

  virtual void action_start ( action_t* );
  virtual void action_miss  ( action_t* );
  virtual void action_hit   ( action_t* );
  virtual void action_tick  ( action_t* );
  virtual void action_damage( action_t*, double amount, int8_t dmg_type );
  virtual void action_finish( action_t* );

  virtual void spell_start_event ( spell_t* );
  virtual void spell_miss_event  ( spell_t* );
  virtual void spell_hit_event   ( spell_t* );
  virtual void spell_tick_event  ( spell_t* );
  virtual void spell_damage_event( spell_t*, double amount, int8_t dmg_type );
  virtual void spell_finish_event( spell_t* );

  virtual void attack_start_event ( attack_t* );
  virtual void attack_miss_event  ( attack_t* );
  virtual void attack_hit_event   ( attack_t* );
  virtual void attack_tick_event  ( attack_t* );
  virtual void attack_damage_event( attack_t*, double amount, int8_t dmg_type );
  virtual void attack_finish_event( attack_t* );

  bool      in_gcd() { return gcd_ready > sim -> current_time; }
  bool      recent_cast();
  void      proc( const std::string& );
  void      gain( const std::string&, double value );
  action_t* find_action( const std::string& );
  void      share_cooldown( const std::string& name, double ready );
  void      share_debuff( const std::string& name, double ready );
  void      recalculate_haste()  {  haste = 1.0 / ( 1.0 + haste_rating / rating.haste ); }
  bool      time_to_think() { return sim -> current_time - last_cast > 0.5; }
  double    spirit_regen_per_second();
  bool      dual_wield();
  void      aura_gain( const char* name );
  void      aura_loss( const char* name );
  
  void parse_talents( talent_translation_t*, const std::string& talent_string );

  virtual void      parse_talents( const std::string& talent_string ) {}
  virtual bool      parse_option ( const std::string& name, const std::string& value );
  virtual action_t* create_action( const std::string& name, const std::string& options ) { return 0; }
  virtual pet_t*    create_pet   ( const std::string& name ) { return 0; }

  virtual ~player_t(){}

  // Class-Specific Methods

  static druid_t   * create_druid  ( sim_t* sim, std::string& name );
  static mage_t    * create_mage   ( sim_t* sim, std::string& name );
  static priest_t  * create_priest ( sim_t* sim, std::string& name );
  static shaman_t  * create_shaman ( sim_t* sim, std::string& name );
  static warlock_t * create_warlock( sim_t* sim, std::string& name );

  druid_t  * druid  () { if( type != DRUID      ) return 0; return (druid_t  *) this; }
  mage_t   * mage   () { if( type != MAGE       ) return 0; return (mage_t   *) this; }
  priest_t * priest () { if( type != PRIEST     ) return 0; return (priest_t *) this; }
  shaman_t * shaman () { if( type != SHAMAN     ) return 0; return (shaman_t *) this; }
  warlock_t* warlock() { if( type != WARLOCK    ) return 0; return (warlock_t*) this; }
  pet_t*     pet    () { if( type != PLAYER_PET ) return 0; return (pet_t    *) this; }
};

// Pet =======================================================================

struct pet_t : public player_t
{
  std::string full_name_str;
  player_t* owner;
  pet_t* next_pet;

  pet_t( sim_t* sim, player_t* owner, const std::string& name );

  virtual const char* name() { return full_name_str.c_str(); }
  virtual void summon();
  virtual void dismiss();
};

// Target ====================================================================

struct target_t
{
  sim_t*      sim;
  std::string name_str;
  int8_t      level;
  int16_t     spell_resistance[ SCHOOL_MAX ];
  int16_t     armor, block_value;
  int8_t      shield;
  double      initial_health, current_health;
  double      total_dmg;

  struct debuff_t
  {
    // Permanent De-Buffs
    int16_t  judgement_of_crusader;
    int8_t   judgement_of_wisdom;
    // Temporary De-Buffs
    int8_t  temporary_debuffs;
    int8_t   affliction_effects;
    int8_t   curse_of_agony;
    int8_t   curse_of_doom;
    int8_t   curse_of_elements;
    int8_t   curse_of_shadows;
    int8_t   fire_vulnerability;
    int8_t   frozen;
    int8_t   mangle;
    int8_t   misery;
    int8_t   misery_stack;
    int8_t   shadow_vulnerability;
    int8_t   shadow_weaving;
    int8_t   sunder_armor;
    int8_t   winters_chill;
    debuff_t() { memset( (void*) this, 0x0, sizeof( debuff_t ) ); }
    void reset()
    { 
      size_t delta = ( (uintptr_t) &temporary_debuffs ) - ( (uintptr_t) this );
      memset( (void*) &temporary_debuffs, 0x0, sizeof( debuff_t ) - delta );
    }
  };
  debuff_t debuffs;
  
  struct expirations_t
  {
    event_t* fire_vulnerability;
    event_t* frozen;
    event_t* shadow_vulnerability;
    event_t* shadow_weaving;
    event_t* winters_chill;
    void reset() { memset( (void*) this, 0x00, sizeof( expirations_t ) ); }
    expirations_t() { reset(); }
  };
  expirations_t expirations;
  
  target_t( sim_t* s );

  void init();
  void reset();
  void assess_damage( double amount, int8_t school, int8_t type );
  void recalculate_health();
  bool parse_option( const std::string& name, const std::string& value );
  const char* name() { return name_str.c_str(); }
};

// Weapon ====================================================================

struct weapon_t
{
  int8_t type, school;
  double damage;
  double swing_time;
  event_t* auto_attack;

  struct enchants_t
  {
    int8_t place_holder;
    void reset() { memset( (void*) this, 0x00, sizeof( enchants_t ) ); }
    enchants_t() { reset(); }
  };
  enchants_t enchants;

  int group();
  double normalized_weapon_speed();

  weapon_t( int t=WEAPON_NONE, double d=0, double st=0, int s=SCHOOL_PHYSICAL ) : 
    type(t), school(s), damage(d), swing_time(st), auto_attack(0) {}
};

// Stats =====================================================================

struct stats_t
{
  std::string name;
  sim_t* sim;
  player_t* player;
  bool channeled;
  stats_t* next;

  double num_executes, num_ticks;
  double total_execute_time, total_tick_time;
  double total_dmg;
  double dps, dpe, dpet;

  struct stats_results_t
  {
    double count, min_dmg, max_dmg, avg_dmg, total_dmg;
  };
  stats_results_t execute_results[ RESULT_MAX ];
  stats_results_t    tick_results[ RESULT_MAX ];

  int num_buckets;
  std::vector<double> timeline_dmg;
  std::vector<double> timeline_dps;

  void add( double amount, int8_t dmg_type, int8_t result, double time );
  void init();
  void analyze();
  stats_t( action_t* action );

  // Necessary for proper accounting of GCD/Lag effects on Damage-Per-Execute-Time:
  static stats_t* last_execute;
  static double   last_execute_time;
  static void adjust_for_gcd_and_lag( double lost_time );
};

// Rank ======================================================================

struct rank_t
{
  int8_t level, index;
  double dd_min, dd_max, dot, cost;
};

// Action ====================================================================

struct action_t
{
  sim_t* sim;
  bool valid;
  int8_t type;
  std::string name_str;
  player_t* player;
  int8_t school, resource, tree, result;
  bool bleed, binary, channeled, background, repeating, aoe, harmful, trigger_gcd;
  bool may_miss, may_resist, may_dodge, may_parry, may_glance, may_block, may_crush, may_crit;
  double base_execute_time, base_duration, base_cost;
  double   base_multiplier,   base_hit,   base_crit,   base_crit_bonus,   base_power,   base_penetration;
  double player_multiplier, player_hit, player_crit, player_crit_bonus, player_power, player_penetration;
  double target_multiplier, target_hit, target_crit, target_crit_bonus, target_power, target_penetration;
  double  dd, base_dd,   dd_power_mod;
  double dot, base_dot, dot_power_mod;
  double dot_tick, time_remaining;
  int8_t num_ticks, current_tick;
  std::string cooldown_group, debuff_group;
  double cooldown, cooldown_ready, debuff_ready;
  stats_t* stats;
  rank_t* rank;
  int8_t rank_index;
  event_t* event;
  double time_to_execute, time_to_tick;
  action_t* next;
  std::vector<action_t*> same_list;

  action_t( int8_t type, const char* name, player_t* p=0, int8_t r=RESOURCE_NONE, int8_t s=SCHOOL_NONE, int8_t t=TREE_NONE );
  virtual ~action_t() {}

  virtual void parse_options( option_t*, const std::string& options_str );
  virtual rank_t* choose_rank( rank_t* rank_list );
  virtual double cost();
  virtual double execute_time() { return base_execute_time; }
  virtual double duration()     { return base_duration;     }
  virtual void player_buff();
  virtual void target_debuff();
  virtual void calculate_result() { assert(0); }
  virtual bool result_is_hit();
  virtual void get_base_damage(); 
  virtual void calculate_damage(); 
  virtual double resistance();
  virtual void adjust_damage_for_result();
  virtual void consume_resource();
  virtual void execute();
  virtual void tick();
  virtual void last_tick();
  virtual void assess_damage( double amount, int8_t dmg_type );
  virtual void schedule_execute();
  virtual void schedule_tick();
  virtual void update_cooldowns();
  virtual void update_stats( int8_t type );
  virtual bool ready();
  virtual void reset();
  virtual const char* name() { return name_str.c_str(); }

  static action_t* create_action( player_t*, const std::string& name, const std::string& options );
};

// Attack ====================================================================

struct attack_t : public action_t
{
  double base_expertise, player_expertise, target_expertise;
  bool normalize_weapon_speed;
  weapon_t* weapon;

  attack_t( const char* n=0, player_t* p=0, int8_t r=RESOURCE_NONE, int8_t s=SCHOOL_PHYSICAL, int8_t t=TREE_NONE );
  virtual ~attack_t() {}

  // Attack Overrides
  virtual double execute_time();
  virtual double duration();
  virtual void   player_buff();
  virtual void   target_debuff();
  virtual void   build_table( std::vector<double>& chances, std::vector<int>& results );
  virtual void   calculate_result();
  virtual void   calculate_damage(); 

  // Passthru Methods
  virtual double cost()                            { return action_t::cost();              }
  virtual void get_base_damage()                   { action_t::get_base_damage();          }
  virtual double resistance()                      { return action_t::resistance();        }
  virtual void adjust_damage_for_result()          { action_t::adjust_damage_for_result(); }
  virtual void consume_resource()                  { action_t::consume_resource();         }
  virtual void execute()                           { action_t::execute();                  }
  virtual void tick()                              { action_t::tick();                     }
  virtual void last_tick()                         { action_t::last_tick();                }
  virtual void assess_damage( double a, int8_t t ) { action_t::assess_damage( a, t );      }
  virtual void schedule_execute()                  { action_t::schedule_execute();         }
  virtual void schedule_tick()                     { action_t::schedule_tick();            }
  virtual void update_cooldowns()                  { action_t::update_cooldowns();         }
  virtual void update_stats( int8_t t )            { action_t::update_stats( t );          }
  virtual bool ready()                             { return action_t::ready();             }
  virtual void reset()                             { action_t::reset();                    }
};

// Spell =====================================================================

struct spell_t : public action_t
{
  spell_t( const char* n=0, player_t* p=0, int8_t r=RESOURCE_NONE, int8_t s=SCHOOL_PHYSICAL, int8_t t=TREE_NONE );
  virtual ~spell_t() {}

  // Spell Overrides
  virtual double execute_time();
  virtual double duration();
  virtual void   player_buff();
  virtual void   target_debuff();
  virtual double level_based_miss_chance( int8_t player, int8_t target );
  virtual void   calculate_result();
   
  // Passthru Methods
  virtual double cost()                            { return action_t::cost();              }
  virtual void get_base_damage()                   { action_t::get_base_damage();          }
  virtual void calculate_damage()                  { action_t::calculate_damage();         }
  virtual double resistance()                      { return action_t::resistance();        }
  virtual void adjust_damage_for_result()          { action_t::adjust_damage_for_result(); }
  virtual void consume_resource()                  { action_t::consume_resource();         }
  virtual void execute()                           { action_t::execute();                  }
  virtual void tick()                              { action_t::tick();                     }
  virtual void last_tick()                         { action_t::last_tick();                }
  virtual void assess_damage( double a, int8_t t ) { action_t::assess_damage( a, t );      }
  virtual void schedule_execute()                  { action_t::schedule_execute();         }
  virtual void schedule_tick()                     { action_t::schedule_tick();            }
  virtual void update_cooldowns()                  { action_t::update_cooldowns();         }
  virtual void update_stats( int8_t t )            { action_t::update_stats( t );          }
  virtual bool ready()                             { return action_t::ready();             }
  virtual void reset()                             { action_t::reset();                    }
};

// Player Ready Event ========================================================

struct player_ready_event_t : public event_t
{
  player_ready_event_t( sim_t* sim, player_t* p, double delta_time );
  virtual void execute();
};

// Action Execute Event ======================================================

struct action_execute_event_t : public event_t
{
  action_t* action;
  action_execute_event_t( sim_t* sim, action_t* a, double time_to_execute );
  virtual void execute();
};

// Action Tick Event =========================================================

struct action_tick_event_t : public event_t
{
  action_t* action;
  action_tick_event_t( sim_t* sim, action_t* a, double time_to_tick );
  virtual void execute();
};

// Regen Event ===============================================================

struct regen_event_t : public event_t
{
   regen_event_t( sim_t* sim );
   virtual void execute();
};

// Unique Gear ===============================================================

struct unique_gear_t
{
  static void spell_start_event ( spell_t* ) {}
  static void spell_miss_event  ( spell_t* );
  static void spell_hit_event   ( spell_t* );
  static void spell_tick_event  ( spell_t* );
  static void spell_damage_event( spell_t*, double amount, int8_t dmg_type ) {}
  static void spell_finish_event( spell_t* );

  static void attack_start_event ( attack_t* ) {}
  static void attack_miss_event  ( attack_t* ) {}
  static void attack_hit_event   ( attack_t* ) {}
  static void attack_tick_event  ( attack_t* ) {}
  static void attack_damage_event( attack_t*, double amount, int8_t dmg_type ) {}
  static void attack_finish_event( attack_t* ) {}

  static action_t* create_action( player_t*, const std::string& name, const std::string& options );
};

// Enchants ===================================================================

struct enchant_t
{
  static void spell_start_event ( spell_t* ) {}
  static void spell_miss_event  ( spell_t* ) {}
  static void spell_hit_event   ( spell_t* ) {}
  static void spell_tick_event  ( spell_t* ) {}
  static void spell_damage_event( spell_t*, double amount, int8_t dmg_type ) {}
  static void spell_finish_event( spell_t* );

  static void attack_start_event ( attack_t* ) {}
  static void attack_miss_event  ( attack_t* ) {}
  static void attack_hit_event   ( attack_t* ) {}
  static void attack_tick_event  ( attack_t* ) {}
  static void attack_damage_event( attack_t*, double amount, int8_t dmg_type ) {}
  static void attack_finish_event( attack_t* ) {}
};

// Consumable ================================================================

struct consumable_t
{
  static action_t* create_action( player_t*, const std::string& name, const std::string& options );
};

// Up-Time ===================================================================

struct uptime_t
{
  std::string name_str;
  int32_t up, down;
  uptime_t* next;
  uptime_t( const std::string& n ) : name_str(n), up(0), down(0) {}
  void   update( bool is_up ) { if( is_up ) up++; else down++; }
  double percentage() { return (up==0) ? 0 : (100.0*up/(up+down)); }
  const char* name() { return name_str.c_str(); }
};

// Report =====================================================================

struct report_t
{
  sim_t* sim;
  int8_t report_actions;
  int8_t report_attack_stats;
  int8_t report_core_stats;
  int8_t report_dpm;
  int8_t report_dps;
  int8_t report_gains;
  int8_t report_miss;
  int8_t report_mps;
  int8_t report_name;
  int8_t report_pq;
  int8_t report_procs;
  int8_t report_raid_dps;
  int8_t report_spell_stats;
  int8_t report_tag;
  int8_t report_uptime;

  report_t( sim_t* s );
  bool parse_option( const std::string& name, const std::string& value );
  void print_actions     ( player_t* );
  void print_gains       ( player_t* );
  void print_procs       ( player_t* );
  void print_core_stats  ( player_t* );
  void print_spell_stats ( player_t* );
  void print_attack_stats( player_t* );
  void print_uptime();
  void print();
  static void timestamp( sim_t* sim );
  static void va_printf( sim_t*, const char* format, va_list );
  inline static void log( sim_t* sim, const char* format, ... )
  {
#if DEBUG
    if( sim -> log ) 
    {
      va_list vap;
      va_start( vap, format );
      va_printf( sim, format, vap );
    }
#endif
  }
  inline static void debug( sim_t* sim, const char* format, ... )
  {
#if DEBUG
    if( sim -> debug ) 
    {
      va_list vap;
      va_start( vap, format );
      va_printf( sim, format, vap );
    }
#endif
  }
};

// Talent Translation =========================================================

struct talent_translation_t
{
  int8_t  index;
  int8_t* address;

  static bool verify( player_t*, const std::string talent_string );
};

// Options ====================================================================

enum option_type_t { OPT_STRING=0, OPT_CHAR_P, OPT_INT8, OPT_INT16, OPT_INT32, OPT_FLT, OPT_UNKNOWN };

struct option_t
{
  char*  name; 
  int8_t type; 
  void*  address;

  static void print( option_t* );
  static bool parse( option_t*, const std::string& name, const std::string& value );
  static bool parse( sim_t*, char* str );
  static bool parse( int argc, char** argv, sim_t* );
};

// Utilities =================================================================

void    wow_random_init( int32_t seed );
int8_t  wow_random( double chance );
double  wow_random();
char*   wow_dup( const char* );

const char*   wow_player_type_string  ( int8_t type );
const char*   wow_dmg_type_string     ( int8_t type );
const char*   wow_result_type_string  ( int8_t type );
const char*   wow_resource_type_string( int8_t type );
const char*   wow_school_type_string  ( int8_t type );
const char*   wow_talent_tree_string  ( int8_t tree );

int wow_string_split( std::vector<std::string>& results, const std::string& str, const char* delim );
int wow_string_split( const std::string& str, const char* delim, const char* format, ... );


