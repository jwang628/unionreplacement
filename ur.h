// ur.h
#ifndef ur_h
#define ur_h

void removeMultiLineComments(char *sqlQuery);
int strncasecmp(const char *s1, const char *s2, size_t n);
void extractSubstring(const char *input, char *output, int position);
int findUnquotedKeyWithSpaces(const char *query, const char *keyword);
void removeUnwantedCharsFromEnd(char *str);
char* urNestedDollOrAgg(const char *fpq, const char *lpq, const char *src, int ors, const int size, const char *term, const char *begn, const char* act, const char* aggred, const char *alias, int FromClausePosition);
void toLowercase(char *str);
char* aggActAggred(char *act, char *aggred, char *alias, const char *src, const int size );
char* urNestedDollOr(const char *fpq, const char *lpq, const char *src, int ors, const int size, const char *term, const char *begn);
char* urOrAgg(const char *fpq, const char *lpq, const char *src, int ors, bool bracked, const int size, const char *term, const char *begn, const char* act, const char* aggred, const char *alias, int FromClausePosition);
char* urOr(const char *fpq, const char *lpq, const char *src, int ors, bool bracked, const int size, const char *term, const char *begn);
int findHowManyOrs (const char* src);
char* extractAfterLastDigitIgnoringUnderscore(const char* src);
char* extractUrhint(const char* query);
int* wholeOrConditions(const char *src, int ors, bool bracked, const char *term, const char *begn);
bool checkIfBracketForOr (const char *src);
char* workflow(const char *content, const char *hints);
#endif
