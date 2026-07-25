// Player template
typedef struct {
  int strength;
  int endurance;
  int constitution;
  int agility;
  int perception;

  int hunger;  // 0 (full) - 100 (starving)
  int thirst;  // 0 (hydrated) - 100 (dehydrated)

  int max_carry_weight;
  int reputation;
} Player;
