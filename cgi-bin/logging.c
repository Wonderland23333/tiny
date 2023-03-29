/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
/*
 * adder.c - a minimal CGI program that adds two numbers together
 */
/* $begin adder */
#include "csapp.h"
#include <stdio.h>
#include <stdlib.h>
#include <sqlite3.h>

#define MAX_USERNAME_LEN 64
#define MAX_PASSWORD_LEN 64

char content[MAXLINE];

int tableop(int n1, int n2) {
    sqlite3 *db;
    sqlite3_stmt *stmt;
    int result = 0;

    // Open the database
    int rc = sqlite3_open("/Users/grander/Downloads/tiny/student.db", &db);
    if (rc != SQLITE_OK) {
        sprintf(content, "Can't open database: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    // Prepare the SQL statement
    const char *sql = "SELECT * FROM stu WHERE id = ? AND key = ?";
    rc = sqlite3_prepare_v2(db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sprintf(content, "Can't prepare SQL statement: %s\n", sqlite3_errmsg(db));
        sqlite3_close(db);
        return 0;
    }

    // Bind the parameters
    int val1 = n1;
    int val2 = n2;
    sqlite3_bind_int(stmt, 1, val1);
    sqlite3_bind_int(stmt, 2, val2);
    //sprintf(content,"%s",sql);
    // Execute the query
    rc = sqlite3_step(stmt);
    if (rc == SQLITE_ROW) {
        // We found a matching row
        result = 1;
    } else if (rc != SQLITE_DONE) {
        sprintf(content, "Can't execute query: %s<p>", sqlite3_errmsg(db));
        sqlite3_finalize(stmt);
        sqlite3_close(db);
        return 0;
    }

    // Clean up
    sqlite3_finalize(stmt);
    sqlite3_close(db);

    if (result) {
        sprintf(content, "%sWelcome!<p>",content);
        return 1;
    } else {
        sprintf(content, "%sAccount or Secret Code is wrong!<p>",content);
        return 0;
    }
}


int main(void) {
    char username[MAX_USERNAME_LEN], password[MAX_PASSWORD_LEN];
    char *buf, *p, *r;
    int n1 = 0, n2 = 0;

    /* Extract the two arguments */
    if ((buf = getenv("QUERY_STRING")) != NULL) {
        // Find the username parameter
        p = strchr(buf, '=');
        if (p != NULL) {
            p++; // Skip the '=' sign
            strcpy(username, p);
            p = strchr(username, '&');
            if (p != NULL) {
                r = p; // Record the '&' position
            }
        }

        // Find the password parameter
        if (p != NULL) {
            p++; // Skip the '&'
            p = strchr(p, '=');
            if (p != NULL) {
                p++; // Skip the '=' sign
                strcpy(password, p);
            }
            *r = '\0'; // Terminate the username string
        }
    }


    n1 = atoi(username);
    n2 = atoi(password);



    /* Make the response body */
    //sprintf(content, "%sWelcome to add.com: ",content);
    //sprintf(content, "%sTHE Internet addition portal.\r\n<p>", content);
    //sprintf(content, "%sThe answer is: %d + %d = %d\r\n<p>", 
    //        content, n1, n2, n1 + n2);
    //sprintf(content, "%sThanks for visiting!\r\n", content);

    // Check if the credentials are valid
    if (tableop(n1, n2) == 1) {
        sprintf(content, "%sLog Success!\r\n", content);
    }else{
        sprintf(content, "%sFalire!\r\n",content);
    }
  
    /* Generate the HTTP response */
    printf("Connection: close\r\n");
    printf("Content-length: %d\r\n", (int)strlen(content));
    printf("Content-type: text/html\r\n\r\n");
    printf("%s", content);
    fflush(stdout);

    exit(0);
}
/* $end adder */
