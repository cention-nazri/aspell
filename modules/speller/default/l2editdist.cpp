
#include "leditdist.hpp"

#define check_rest(a,b,w)               \
          a0 = a; b0 = b;               \
          while(*a0 == *b0) {           \
	    if (*a0 == '\0') {          \
	      if (w < min) min = w;     \
	      break;                    \
	    }                           \
	    ++a0;                       \
	    ++b0;                       \
          }                             \
          if (amax < a0) amax = a0;

#define check2(a,b,w)                                             \
  aa = a; bb = b;                                                 \
  while(*aa == *bb) {                                             \
    if (*aa == '\0')  {                                           \
      if (amax < aa) amax = aa;                                   \
      if (w < min) min = w;                                       \
      break;                                                      \
    }                                                             \
    ++aa; ++bb;                                                   \
  }                                                               \
  if (*aa == '\0') {                                              \
    if (amax < aa) amax = aa;                                     \
    if (*bb == '\0') {}                                           \
    else if (*(bb+1) == '\0' && w+ws.del2 < min) min = w+ws.del2; \
  } else if (*bb == '\0') {                                       \
    ++aa;                                                         \
    if (amax < aa) amax = aa;                                     \
    if (*aa == '\0' && w+ws.del1 < min) min = w+ws.del1;          \
  } else {                                                        \
    check_rest(aa+1,bb,w+ws.del1);                                \
    check_rest(aa,bb+1,w+ws.del2);                                \
    if (*aa == *(bb+1) && *bb == *(aa+1)) {                       \
      check_rest(aa+2,bb+2,w+ws.swap);                            \
    } else {                                                      \
      check_rest(aa+1,bb+1,w+ws.sub);                             \
    }                                                             \
  }

namespace aspeller {

  EditDist limit1_edit_distance(const char * a, const char * b,
				const EditDistanceWeights & ws)
  {
    int min = LARGE_NUM;
    const char * a0;
    const char * b0;
    const char * amax = a;
    
    while(*a == *b) { 
      if (*a == '\0') 
	return EditDist(0, a);
      ++a; ++b;
    }

    if (*a == '\0') {
      
      ++b;
      if (*b == '\0') return EditDist(ws.del2, a);
      return EditDist(LARGE_NUM, a);
      
    } else if (*b == '\0') {

      ++a;
      if (*a == '\0') return EditDist(ws.del1, a);
      return EditDist(LARGE_NUM, a);
      
    } else {
      
      // delete a character from a
      check_rest(a+1,b,ws.del1);
      
      // delete a character from b
      check_rest(a,b+1,ws.del2);

      if (*a == *(b+1) && *b == *(a+1)) {
	
	// swap two characters
	check_rest(a+2,b+2,ws.swap);

      } else {
	
	// substitute one character for another which is the same
	// thing as deleting a character from both a & b
	check_rest(a+1,b+1,ws.sub);
	
      }
    }
    return EditDist(min, amax);
  }

  EditDist limit2_edit_distance(const char * a, const char * b,
				const EditDistanceWeights & ws)
  {
    int min = LARGE_NUM;
    const char * a0;
    const char * b0;
    const char * aa;
    const char * bb;
    const char * amax = a;
    
    while(*a == *b) { 
      if (*a == '\0') 
	return EditDist(0, a);
      ++a; ++b;
    }

    if (*a == '\0') {
      
      ++b;
      if (*b == '\0') return EditDist(ws.del2, a);
      ++b;
      if (*b == '\0') return EditDist(2*ws.del2, a);
      return EditDist(LARGE_NUM, a);
      
    } else if (*b == '\0') {

      ++a;
      if (*a == '\0') return EditDist(ws.del1, a);
      ++a;
      if (*a == '\0') return EditDist(2*ws.del1, a);
      return EditDist(LARGE_NUM, a);
      
    } else {
      
      // delete a character from a
      check2(a+1,b,ws.del1);
      
      // delete a character from b
      check2(a,b+1,ws.del2);

      if (*a == *(b+1) && *b == *(a+1)) {
	
	// swap two characters
	check2(a+2,b+2,ws.swap);

      } else {
	
	// substitute one character for another which is the same
	// thing as deleting a character from both a & b
	check2(a+1,b+1,ws.sub);
	
      }
    }
    return EditDist(min, amax);
  }

}

