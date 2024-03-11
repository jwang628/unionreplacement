#include "postgres.h"
#include <ctype.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <regex.h>
#include <stdbool.h>  // Include the <stdbool.h> header
#include <stdlib.h>
#include <stddef.h> // Include for ptrdiff_t
#include <unistd.h> // For sleep() usleep()
#include <time.h>
#include "ur.h"

//global
#define MAX_ORS 32 //for regmatch_t
//remove comments in queries
void removeMultiLineComments(char *sqlQuery) {
    char *output = sqlQuery; // Output index to write characters to.
    bool inComment = false;

    while (*sqlQuery != '\0') {
        if (!inComment && *sqlQuery == '/' && *(sqlQuery + 1) == '*') {
            // Start of a comment
            inComment = true;
            sqlQuery += 2; // Skip the comment start
        } else if (inComment && *sqlQuery == '*' && *(sqlQuery + 1) == '/') {
            // End of a comment
            inComment = false;
            sqlQuery += 2; // Skip the comment end
        } else if (!inComment) {
            // Copy characters outside of comments
            *output++ = *sqlQuery++;
        } else {
            // Inside a comment, just move to the next character
            sqlQuery++;
        }
    }
    *output = '\0'; // Null terminate the output string
}

int strncasecmp(const char *s1, const char *s2, size_t n) {
    while (n > 0 && (tolower((unsigned char)*s1) == tolower((unsigned char)*s2))) {
        if (*s1 == '\0' || *s2 == '\0') {
            break;
        }
        s1++;
        s2++;
        n--;
    }
    if (n > 0) {
        return tolower((unsigned char)*s1) - tolower((unsigned char)*s2);
    }
    return 0;
}
//extract string to the position 
void extractSubstring(const char *input, char *output, int position) {
    int length = strlen(input);

    if (position >= 0 && position < length) {
        strncpy(output, input, position);
        output[position] = '\0'; // Null-terminate the output string
    } else {
        elog(WARNING, "Invalid extraction position.");
        output[0] = '\0'; // Empty string if the position is invalid
    }
}

int findUnquotedKeyWithSpaces(const char *query, const char *keyword) {
    char quote = '\0';  // Initialize to no quote
    int i;
    int keylen=strlen(keyword);
    for (i = 0; query[i] != '\0'; i++) {
        if (query[i] == '\'' || query[i] == '\"') {
            // Found a quote character
            if (quote == '\0') {
                // Quote has not been set, set it
                quote = query[i];
            } else if (quote == query[i]) {
                // Quote matches, unset it (closing quote)
                quote = '\0';
            }
        }

        if (query[i] == ' ' && strncasecmp(query + i + 1, keyword, keylen) == 0 && quote == '\0') {
            // Found the substring " OR " not quoted
            return i;
        }
    }

    // " or " is not found or is quoted
    return -1;
}

int* wholeOrConditions(const char *src, int ors, bool bracked, const char *term, const char *begn) {
    static int or_pos_len[] = {0,0};
    const char *base;
    char pattern[4096]; // Adjust the size as needed
    regex_t regex;
    regmatch_t matches[MAX_ORS + 2]; // Two pairs for each OR condition and one for the entire match
    int status;
    if (strstr(begn, "AND")) {base = " and \\((.*";}
    else if (strstr(begn, " (")) {base = " \\((.*";}
    else { base = "(.*"; }
    strcpy(pattern, base);
    // Construct the pattern with multiple OR conditions
    for (int i = 0; i < ors; i++) {
        strcat(pattern, "\\s+or\\s+.*");
    }
    if (strstr(begn, " (")) {
        strcat(pattern, ")");
        if ( 0 == strncmp(term, " AND", 4)) { strcat(pattern, ")");}
        strcat(pattern, term);
    } else {
        strcat(pattern, ")");
        strcat(pattern, term);
    }
    elog(DEBUG1, "wholeOrConditions pattern %s", pattern);
    // Compile the regular expression
    status = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE);
    if (status != 0) {
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex compilation error: %s", error_message);
        return or_pos_len;
    }
    // Execute the regular expression
    status = regexec(&regex, src, ors + 2, matches, 0);
  
    if (status == 0) {
        // Match found, extract the substrings and print positions and length
        for (int i = 1; i <= ors + 1; ++i) {
            if (1==i) {
                or_pos_len[0]=matches[i].rm_so;
                or_pos_len[1]=(int)(matches[i].rm_eo - matches[i].rm_so);
            }
            elog(DEBUG1, "Match %d: %.*s (Position: %d, Length: %d)", i,
                   (int)(matches[i].rm_eo - matches[i].rm_so),
                   src + matches[i].rm_so, matches[i].rm_so,
                   (int)(matches[i].rm_eo - matches[i].rm_so));
        }
    } else if (status == REG_NOMATCH) {
        elog(WARNING, "No match found in wholeOrConditions. src %s\nwholeOrConditions pattern %s\nwholeOrConditions ors %d", src, pattern, ors);
    } else {
        // Other error
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex execution error: %s", error_message);
    }

    // Free the compiled regular expression
    regfree(&regex);
    return or_pos_len;
}

void removeUnwantedCharsFromEnd(char *str) {
    int len = strlen(str);
    bool tocontinue=false;
    while (true) {
        if (str[len-1] == ' ' || str[len-1] == '\n') {
            str[len-1] = '\0';
            len --;
            tocontinue=true;
        } else { 
            if (str[len-1] == ';') {
                str[len-1] = '\0';
            } 
            if (str[len-2] == '\\' && str[len-1] == 'G' ) {
                str[len-2] = '\0';
            }
            if (str[len-3] == '\\' && str[len -2] == 'g' && str[len -1] == 'x') {
                str[len-3] = '\0';
            }
            tocontinue=false;
        }
        elog(DEBUG1, "looping tocontinue %d len %d str[%d-1] %c", tocontinue, len, len, str[len-1]);
        if (! tocontinue) break;
    }
}
//Russian Matryoshka dolls OR logic UR philips 1332516621 aggregation count(*) max min average et al
char* urNestedDollOrAgg(const char *fpq, const char *lpq, const char *src, int ors, const int size, const char *term, const char *begn, const char* act, const char* aggred, const char *alias, int FromClausePosition) {
    char *newfpq = (char *)malloc(strlen(fpq)+80);
    char *newQuery = (char *)malloc((ors+1)*(size+8));
    const char *base;
    char pattern[4096]; // Adjust the size as needed
    regex_t regex;
    regmatch_t matches[MAX_ORS + 2]; // Two pairs for each OR condition and one for the entire match
    int status;
    sprintf(newfpq, "SELECT %s as ured %s", aggred, &fpq[FromClausePosition]);
    sprintf(newQuery, "SELECT count(ured) as %s FROM (", alias);
    elog(DEBUG1, "newQuery %s\nnewfpq %s", newQuery, newfpq);
    base = "\\((.*)"; //nested all bracked 
    if (strstr(begn, "AND")) {base = " and \\((.*)";}
    else { base = "\\((.*)"; } 

    strcpy(pattern, base);
    // Construct the pattern with multiple OR conditions
    for (int i = 0; i < ors; i++) {
        strcat(pattern, "\\s+or\\s+(.*)");
    }
    strcat(pattern, ")");
    strcat(pattern, term);

    elog(DEBUG1, "urNestedDollOrAgg pattern %s", pattern);
    // Compile the regular expression
    status = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE);
    if (status != 0) {
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex compilation error: %s", error_message);
        return "";
    }
    // Execute the regular expression
    status = regexec(&regex, src, ors + 2, matches, 0);
  
    if (status == 0) {
        // Match found, extract the substrings and print positions and length
        for (int i = 1; i <= ors + 1; ++i) {
            size_t substringLength = matches[i].rm_eo - matches[i].rm_so;
            // Allocate space for the substring + null terminator
            char *substring = malloc(substringLength + 1);
            if (substring != NULL) {
                strncpy(substring, src + matches[i].rm_so, substringLength);
                // Null-terminate the substring
                substring[substringLength] = '\0';

                // Concatenate the substring to newQuery
                if (i > 1) newQuery = strcat(newQuery, "\n UNION \n");
                newQuery = strcat(newQuery, "(");
                newQuery = strcat(newQuery, newfpq);
                newQuery = strcat(newQuery, substring);
                newQuery = strcat(newQuery, ")");
                newQuery = strcat(newQuery, lpq);
                newQuery = strcat(newQuery, ")");
                free(substring);
            }
            elog(DEBUG1, "urNestedDollOrAgg Match %d: %.*s (Position: %d, Length: %d)", i,
                   (int)(matches[i].rm_eo - matches[i].rm_so),
                   src + matches[i].rm_so, matches[i].rm_so,
                   (int)(matches[i].rm_eo - matches[i].rm_so));
        }
    } else if (status == REG_NOMATCH) {
        elog(WARNING, "No match found in urNestedDollOrAgg. src %s\nurNestedDollOrAgg pattern %s\nurNestedDollOrAgg ors %d", src, pattern, ors);
    } else {
        // Other error
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex execution error: %s", error_message);
    }

    newQuery = strcat(newQuery, ") combined");
    // Free the compiled regular expression
    regfree(&regex);
    return newQuery;
}
// Function to convert a string to lowercase
void toLowercase(char *str) {
    for (; *str; ++str) *str = tolower((unsigned char)*str);
}

//extract aggregation method, aggregated column
char* aggActAggred(char *act, char *aggred, char *alias, const char *src, const int size ) {
    int ors= 1;
    regex_t regex;
    regmatch_t matches[MAX_ORS + 3]; // Two pairs for each OR condition and one for the entire match
    char pattern[4096]; // Adjust the size as needed
    char *srcLower;
    int status;
    srcLower = (char *) malloc ((strlen(src) + 1) * sizeof(char));
    strcpy(pattern, "");
    strcpy(srcLower, src);
    toLowercase(srcLower);

    if (strstr(srcLower, " as ")) {
        ors = 2;
        strcat(pattern, " ([^( ]+)\\(([^ ]+))\\s+as ([^ ]+)");
    } else {
        strcat(pattern, " ([^( ]+)\\(([^ ]+))");
    }

    elog(DEBUG1, "aggActAggred pattern %s", pattern);
    // Compile the regular expression
    status = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE);
    if (status != 0) {
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex compilation error: %s", error_message);
        return "";
    }
    // Execute the regular expression
    status = regexec(&regex, src, ors + 3, matches, 0);
  
    if (status == 0) {
        // Match found, extract the substrings and print positions and length
        for (int i = 1; i <= ors + 1; ++i) {
            size_t substringLength = matches[i].rm_eo - matches[i].rm_so;
            // Allocate space for the substring + null terminator
            char *substring = malloc(substringLength + 1);
            if (substring != NULL) {
                strncpy(substring, src + matches[i].rm_so, substringLength);
                // Null-terminate the substring
                substring[substringLength] = '\0';

                // Concatenate the substring to newQuery
                if (i == 1) strcpy(act, substring);
                if (i == 2) strcpy(aggred, substring);
                if (i == 3) strcpy(alias, substring);
            }
            elog(DEBUG1, "aggActAggred Match %d: %.*s (Position: %d, Length: %d)", i,
                   (int)(matches[i].rm_eo - matches[i].rm_so),
                   src + matches[i].rm_so, matches[i].rm_so,
                   (int)(matches[i].rm_eo - matches[i].rm_so));
        }
    } else if (status == REG_NOMATCH) {
        elog(WARNING, "No match found in aggActAggred. src %s\naggActAggred pattern %s\naggActAggred ors %d", src, pattern, ors);
    } else {
        // Other error
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex execution error: %s", error_message);
    }

    // Free the compiled regular expression
    regfree(&regex);
    return "";
}
//Russian Matryoshka dolls OR logic UR philips 1332516621
char* urNestedDollOr(const char *fpq, const char *lpq, const char *src, int ors, const int size, const char *term, const char *begn) {
    char pattern[4096]; // Adjust the size as needed
    const char *base;
    regex_t regex;
    regmatch_t matches[MAX_ORS + 2]; // Two pairs for each OR condition and one for the entire match
    int status;
    char *newQuery = (char *)malloc((ors+1)*(size+8));
    strcpy(newQuery, "");
    base = "\\((.*)"; //nested all bracked 
    if (strstr(begn, "AND")) {base = " and \\((.*)";}
    else { base = "\\((.*)"; } 

    strcpy(pattern, base);
    // Construct the pattern with multiple OR conditions
    for (int i = 0; i < ors; i++) {
        strcat(pattern, "\\s+or\\s+(.*)");
    }
    strcat(pattern, ")");
    strcat(pattern, term);

    elog(DEBUG1, "urNestedDollOr pattern %s", pattern);
    // Compile the regular expression
    status = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE);
    if (status != 0) {
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex compilation error: %s", error_message);
        return "";
    }
    // Execute the regular expression
    status = regexec(&regex, src, ors + 2, matches, 0);
  
    if (status == 0) {
        // Match found, extract the substrings and print positions and length
        for (int i = 1; i <= ors + 1; ++i) {
            size_t substringLength = matches[i].rm_eo - matches[i].rm_so;
            // Allocate space for the substring + null terminator
            char *substring = malloc(substringLength + 1);
            if (substring != NULL) {
                strncpy(substring, src + matches[i].rm_so, substringLength);
                // Null-terminate the substring
                substring[substringLength] = '\0';

                // Concatenate the substring to newQuery
                if (i > 1) newQuery = strcat(newQuery, "\n UNION \n");
                newQuery = strcat(newQuery, "(");
                newQuery = strcat(newQuery, fpq);
                newQuery = strcat(newQuery, substring);
                newQuery = strcat(newQuery, ")");
                newQuery = strcat(newQuery, lpq);
                newQuery = strcat(newQuery, ")");
                free(substring);
            }
            elog(DEBUG1, "urNestedDollOr Match %d: %.*s (Position: %d, Length: %d)", i,
                   (int)(matches[i].rm_eo - matches[i].rm_so),
                   src + matches[i].rm_so, matches[i].rm_so,
                   (int)(matches[i].rm_eo - matches[i].rm_so));
        }
    } else if (status == REG_NOMATCH) {
        elog(WARNING, "No match found in urNestedDollOr. src %s\nurNestedDollOr pattern %s\nurNestedDollOr ors %d", src, pattern, ors);
    } else {
        // Other error
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex execution error: %s", error_message);
    }

    // Free the compiled regular expression
    regfree(&regex);
    return newQuery;
}
char* urOrAgg(const char *fpq, const char *lpq, const char *src, int ors, bool bracked, const int size, const char *term, const char *begn, const char* act, const char* aggred, const char *alias, int FromClausePosition) {
    const char *base;
    char pattern[4096]; // Adjust the size as needed
    regex_t regex;
    regmatch_t matches[MAX_ORS + 2]; // Two pairs for each OR condition and one for the entire match
    int status;
    char *newfpq = (char *)malloc(strlen(fpq)+80);
    char *newQuery = (char *)malloc((ors+1)*(size+80));
    if (strstr(aggred, "*") || (1 == strlen(aggred))) {
        if (strlen(alias) > 0) {
            sprintf(newfpq, "%s", fpq);
            sprintf(newQuery, "SELECT sum(%s) as %s FROM (", alias, alias);
        } else {
            sprintf(newfpq, "SELECT %s(%s) as ured %s", act, aggred, &fpq[FromClausePosition]);
            sprintf(newQuery, "SELECT sum(ured) as %s FROM (", act);
        }
    } else {
        sprintf(newfpq, "SELECT %s as ured %s", aggred, &fpq[FromClausePosition]);
        sprintf(newQuery, "SELECT count(ured) as %s FROM (", alias);
    }
    elog(DEBUG1, "newQuery %s\nnewfpq %s", newQuery, newfpq);
    if (strstr(begn, "AND")) {base = " and \\((.*)";}
    else if (strstr(begn, "WHERE")) {
        if (bracked) base = " where \\((.*)";
        else base = " where (.*)";
    }
    else if (strstr(begn, " ("))  {base = " \\((.*)";}
    else { base = "(.*)"; }
    strcpy(pattern, base);
    // Construct the pattern with multiple OR conditions
    for (int i = 0; i < ors; i++) {
        if (bracked)  
            strcat(pattern, "\\s+or\\s+(\\(.*)");
        else
            strcat(pattern, "\\s+or\\s+(.*)");
    }
    if (strstr(begn, " ("))  {
        elog(DEBUG1, "begn %s", begn);
        if ( 0 == strncmp(term, " AND", 4)) { strcat(pattern, ")");}
        strcat(pattern, term);
    } else {
        elog (DEBUG1, "term %s strncmp %d", term, strncmp(term, " AND", 4));
        //strcat(pattern, ")");
        strcat(pattern, term);
    }

    elog(DEBUG1, "urOrAgg pattern %s", pattern);
    // Compile the regular expression
    status = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE);
    if (status != 0) {
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "urOrAgg Regex compilation error: %s", error_message);
        return "";
    }
    // Execute the regular expression
    status = regexec(&regex, src, ors + 2, matches, 0);
  
    if (status == 0) {
        // Match found, extract the substrings and print positions and length
        for (int i = 1; i <= ors + 1; ++i) {
            size_t substringLength = matches[i].rm_eo - matches[i].rm_so;
            // Allocate space for the substring + null terminator
            char *substring = malloc(substringLength + 1);
            if (substring != NULL) {
                strncpy(substring, src + matches[i].rm_so, substringLength);
                // Null-terminate the substring
                substring[substringLength] = '\0';

                // Concatenate the substring to newQuery
                if (i > 1) newQuery = strcat(newQuery, "\n UNION \n");
                newQuery = strcat(newQuery, "(");
                newQuery = strcat(newQuery, newfpq);
                if (strstr(begn, "WHERE")) newQuery = strcat(newQuery, " WHERE ");
                newQuery = strcat(newQuery, substring);
                if ( 0 != strncmp(term, " AND", 4)) newQuery = strcat(newQuery, ")");
                newQuery = strcat(newQuery, lpq);
                if ( 0 != strcmp(term, ")") && ! strstr(begn, "WHERE")) newQuery = strcat(newQuery, ")");
                // Don't forget to free the allocated memory for the substring
                free(substring);
            }
            elog(DEBUG1, "urOrAgg Match %d: %.*s (Position: %d, Length: %d)", i,
                   (int)(matches[i].rm_eo - matches[i].rm_so),
                   src + matches[i].rm_so, matches[i].rm_so,
                   (int)(matches[i].rm_eo - matches[i].rm_so));
        }
    } else if (status == REG_NOMATCH) {
        elog(WARNING, "No match found urOrAgg. src %s\nurOrAgg pattern %s\nurOrAgg ors %d", src, pattern, ors);
    } else {
        // Other error
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "urOrAgg Regex execution error: %s", error_message);
    }
    newQuery = strcat(newQuery, ") combined");
    // Free the compiled regular expression
    regfree(&regex);
    return newQuery;
}
char* urOr(const char *fpq, const char *lpq, const char *src, int ors, bool bracked, const int size, const char *term, const char *begn) {
    const char *base;
    char pattern[4096]; // Adjust the size as needed
    regex_t regex;
    regmatch_t matches[MAX_ORS + 2]; // Two pairs for each OR condition and one for the entire match
    int status;
    char *newQuery = (char *)malloc((ors+1)*(size+8));
    strcpy(newQuery, "");
    if (strstr(begn, "AND")) {base = " and \\((.*)";}
    else if (strstr(begn, "WHERE")) {
        if (bracked) base = " where \\((.*)";
        else base = " where (.*)";
    }
    else if (strstr(begn, " ("))  {base = " \\((.*)";}
    else { base = "(.*)"; }
    strcpy(pattern, base);
    // Construct the pattern with multiple OR conditions
    for (int i = 0; i < ors; i++) {
        if (bracked)  
            strcat(pattern, "\\s+or\\s+(\\(.*)");
        else
            strcat(pattern, "\\s+or\\s+(.*)");
    }
    if (strstr(begn, " ("))  {
        elog(DEBUG1, "begn %s", begn);
        if ( 0 == strncmp(term, " AND", 4)) { strcat(pattern, ")");}
        strcat(pattern, term);
    } else {
        elog(DEBUG1, "term %s strncmp %d", term, strncmp(term, " AND", 4));
        //strcat(pattern, ")");
        strcat(pattern, term);
    }

    elog(DEBUG1, "urOr pattern %s", pattern);
    // Compile the regular expression
    status = regcomp(&regex, pattern, REG_EXTENDED | REG_ICASE);
    if (status != 0) {
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex compilation error: %s", error_message);
        return "";
    }
    // Execute the regular expression
    status = regexec(&regex, src, ors + 2, matches, 0);
  
    if (status == 0) {
        // Match found, extract the substrings and print positions and length
        for (int i = 1; i <= ors + 1; ++i) {
            size_t substringLength = matches[i].rm_eo - matches[i].rm_so;
            // Allocate space for the substring + null terminator
            char *substring = malloc(substringLength + 1);
            if (substring != NULL) {
                strncpy(substring, src + matches[i].rm_so, substringLength);
                // Null-terminate the substring
                substring[substringLength] = '\0';

                // Concatenate the substring to newQuery
                if (i > 1) newQuery = strcat(newQuery, "\n UNION \n");
                newQuery = strcat(newQuery, "(");
                newQuery = strcat(newQuery, fpq);
                if (strstr(begn, "WHERE")) newQuery = strcat(newQuery, " WHERE ");
                newQuery = strcat(newQuery, substring);
                if ( 0 != strncmp(term, " AND", 4)) newQuery = strcat(newQuery, ")");
                if ( strstr(begn, "AND")) newQuery = strcat(newQuery, ")");
                newQuery = strcat(newQuery, lpq);
                if ( 0 != strcmp(term, ")") && ! strstr(begn, "WHERE")) newQuery = strcat(newQuery, ")");
                // Don't forget to free the allocated memory for the substring
                free(substring);
            }
            elog(DEBUG1, "Match %d: %.*s (Position: %d, Length: %d)", i,
                   (int)(matches[i].rm_eo - matches[i].rm_so),
                   src + matches[i].rm_so, matches[i].rm_so,
                   (int)(matches[i].rm_eo - matches[i].rm_so));
        }
    } else if (status == REG_NOMATCH) {
        elog(WARNING, "No match found urOr. src %s\nurOr pattern %s\nurOr ors %d", src, pattern, ors);
    } else {
        // Other error
        char error_message[100];
        regerror(status, &regex, error_message, sizeof(error_message));
        elog(WARNING, "Regex execution error: %s", error_message);
    }

    // Free the compiled regular expression
    regfree(&regex);
    return newQuery;
}

bool checkIfBracketForOr (const char *src) {
    //nst char *pattern = "and\\s+\\(\\s*([^=]+\\.[^=]+='[^']*'\\s+or\\s+[^=]+\\.[^=]+='[^']*')\\)"; //can not deal with IS , LIKE et al
    const char *pattern = "and\\s+\\(\\s*([^(]+\\s+or\\s+)";
    regex_t regex;
    int reti;

    // Compile the regular expression
    reti = regcomp(&regex, pattern, REG_EXTENDED | REG_NOSUB | REG_ICASE);
    if (reti) {
        elog(WARNING, "Could not compile regex");
        return false;
    }

    // Execute the regular expression
    reti = regexec(&regex, src, 0, NULL, 0);
    if (!reti) {
        elog(WARNING, "bracket found in the string.");
        return true;
    } else if (reti == REG_NOMATCH) {
        elog(DEBUG1, "bracket not found in the src %s\n checkIfBracketForOr pattern %s ", src, pattern);
        return false;
    } else {
        char errorBuffer[100];
        regerror(reti, &regex, errorBuffer, sizeof(errorBuffer));
        elog(WARNING, "Regex match failed: %s", errorBuffer);
        return false;
    }
}
/*
    const char *query = "select count(1), ' injected where injected from injected or injected and where ' from table1 t1 join table2 t2 on (t2.id=t1.id) where t1.col1='1' and (t1.col2='2' or t2.col3='3') AND t2.col4='4' group by t2.col5 order by 1 desc limit 10;";
}
*/
int findHowManyOrs (const char* src) {
    const char* p = src;
    int number=0;
    // Find the first digit in the string
    while (*p && !isdigit((unsigned char)*p)) {
        p++;
    }

    // If a digit is found, extract the number
    if (*p) {
        char *end;
        number = strtol(p, &end, 10); // 10 for base-10 (decimal) numbers

        // Optionally, check if the conversion stopped at a non-digit character
        if (p != end) {
            elog(DEBUG1, "Extracted number: %d", number);
        } else {
            elog(WARNING, "Failed to extract the number from the string.");
        }
    } else {
        elog(WARNING, "No digits found in the string.");
    }

    return number;
}

//extract the terminator
char* extractAfterLastDigitIgnoringUnderscore(const char* src) {
    const char* lastDigit = NULL;
    char* extractedPart;

    // Iterate over the string to find the last digit
    for (const char* p = src; *p; p++) {
        if (*p >= '0' && *p <= '9') {
            lastDigit = p;
        }
    }

    // If a digit was found, move the pointer forward past any underscores
    if (lastDigit != NULL) {
        const char* start = lastDigit + 1;
        while (*start == '_') {
            start++;
        }

        // Allocate memory for the extracted part +1 for null terminator
        extractedPart = malloc(strlen(start) + 1);
        if (extractedPart) {
            // Copy the substring after the last digit and underscore to the allocated memory
            strcpy(extractedPart, start);
            return extractedPart;
        } else {
            // Memory allocation failed
            elog(WARNING, "Memory allocation failed");
            return NULL;
        }
    } else {
        // No digit found in the string
        return NULL;
    }
}

//extract urhint from query file
char* extractUrhint(const char* query) {
    char *start, *end;
    static char word[100]; // Assuming the word won't be longer than 99 characters for simplicity.

    // Find the start of the hint
    start = strstr(query, "urhint: ");
    if (start != NULL) {
        // Move start to the beginning of the word we want to extract
        start += strlen("urhint: ");
        // Find the end of the word (assuming it ends with '*/')
        end = strstr(start, "*/");
        if (end != NULL && end > start) {
            // Calculate the length of the word and copy it
            ptrdiff_t len = end - start;
            if (len < (ptrdiff_t)sizeof(word)) {
                strncpy(word, start, len);
                word[len] = '\0'; // Null-terminate the extracted word
                elog(DEBUG1, "Extracted word: %s", word);
            } else {
                elog(WARNING, "The word is too long to fit in the buffer.");
            }
        } else {
            elog(WARNING, "Couldn't find the end of the hint.");
        }
    } else {
        elog(WARNING, "Hint %s not found in the query %s", word, query);
    }

    return word;
}

#define MAX_QUERY_SIZE 32768 // 32M crashed server

char* workflow(const char *query, const char *hints) {
	char content[MAX_QUERY_SIZE];
    const char *lpq;
    char act[11]; char aggred[128]; char alias[32];
    int  ors; 
    int* or_pos_len;
    bool bracked; //default [0-9a-z] OR [0-9a-z] if bracked, ) OR (
    char begn[11]; //nesteddoll_and_4_group begin of the OR condition
    char term[11]; //terminator of the OR condition
    char* tmpt;
    int FromClausePosition;
    const char *mpq;
    char fpq[MAX_QUERY_SIZE];
    char slq[MAX_QUERY_SIZE];
    int WhereClausePosition;
    ors=1; //default 1 OR
	strcpy(content, query);

    if (NULL == hints || strstr(hints, "debug")) { hints=extractUrhint(content); }
    removeMultiLineComments(content);
    removeUnwantedCharsFromEnd(content);
    WhereClausePosition = findUnquotedKeyWithSpaces(content, "WHERE ");

    if (WhereClausePosition != -1) {
        elog(DEBUG1, "Position of ' where ': %d '", WhereClausePosition);
    } else {
        elog(DEBUG1, "No unquoted ' where ' found.");
    }
    mpq=&content[WhereClausePosition]; //content after where
    FromClausePosition = findUnquotedKeyWithSpaces(content, "FROM ");
    bracked=false; //default [0-9a-z] OR [0-9a-z] if bracked, ) OR (
    strcpy(begn, ""); //nesteddoll_and_4_group begin of the OR condition
    strcpy(term, ""); //terminator of the OR condition
    if (strstr(hints, "_and_")) strcpy(begn, " AND ");
    if (strstr(hints, "_where_")) strcpy(begn, " WHERE ");
    if (strstr(hints, "_(_")) {
        strcpy(begn, " (");
        strcpy(term, ")");
    }
    if (strstr(hints, "bracked")) bracked=true; 
    if (!strstr(hints, "debug") && 0 == strcmp("", hints)) {
        ors=findHowManyOrs(hints);
        tmpt=extractAfterLastDigitIgnoringUnderscore(hints);
        if (strstr(tmpt, ")"))    strcpy(term, ")");
        if (strstr(tmpt, "and"))   strcpy(term, " AND ");
        if (strstr(tmpt, "group")) strcpy(term, " GROUP BY ");
        if (strstr(tmpt, "order")) strcpy(term, " ORDER BY ");
        if (strstr(tmpt, "limit")) strcpy(term, " LIMIT ");
    }
    or_pos_len=wholeOrConditions(mpq, ors, bracked, term, begn); //ors should not be hard coded
    extractSubstring(content, fpq, WhereClausePosition + or_pos_len[0]);
    extractSubstring(content, slq, FromClausePosition);
    strcpy(act, ""); strcpy(aggred, ""); strcpy(alias, "");
    if (strstr(slq, " count") || strstr(slq, " COUNT")) {
        aggActAggred(act, aggred, alias, slq, 0 );
        elog(DEBUG1, "act %s aggred %s alias %s\nslq %s", act, aggred, alias,slq);
    }
	lpq=&content[WhereClausePosition + or_pos_len[0] + or_pos_len[1]]; //last part of the query
    elog(DEBUG1, "hints %s bracked %d ors %d term %s begn %s or_pos %d or_len %d", hints, bracked, ors, term, begn, or_pos_len[0], or_pos_len[1]);
    elog(DEBUG1, "act len %ld FromClausePosition %d fpq %s\nlpq %s", strlen(act), FromClausePosition, fpq, lpq);
    
    // Write the modified string to the file
    if (strstr(hints, "nesteddoll")) {
        if (strlen(act) > 1) return urNestedDollOrAgg(fpq, lpq, mpq, ors, (int)strlen(content)+1, term, begn, act, aggred, alias, FromClausePosition); 
        else return urNestedDollOr(fpq, lpq, mpq, ors, (int)strlen(content)+1, term, begn); 
    } else  {
        if (strlen(act) > 1) return urOrAgg(fpq, lpq, mpq, ors, bracked, (int)strlen(content)+1, term, begn, act, aggred, alias, FromClausePosition); 
        else return urOr(fpq, lpq, mpq, ors, bracked, (int)strlen(content)+1,  term, begn); 
    }
	return "";
}
