#include <iostream>
#include "sqlite3.h"
using namespace std;
int main(){
    char *err;
sqlite3 *db;
sqlite3_stmt *stmt;
int check=sqlite3_open("wtf.db",&db);
if(check){
    cerr<<"Error Opening Database!!!: "<<sqlite3_errmsg(db)<<endl;
return check;
}

//CREATING TABLE
check = sqlite3_exec(db,"CREATE TABLE IF NOT EXISTS mess(id INTEGER PRIMARY KEY,"
                                                            "sen_name TEXT,"
                                                            "rec_name TEXT,"
                                                            "mess TEXT,"
                                                            "timestamp DATETIME DEFAULT CURRENT_TIMESTAMP);",NULL,NULL,&err);
   if(check!=SQLITE_OK){
    cerr<<"Error Creating Table!!!: "<<err<<endl;
return check;}                                                         

//Input the data
string sen_name,rec_name,mess;
cout<<"Enter the sender name: ";
getline(cin,sen_name);
cout<<"Enter the receiver name: ";
getline(cin,rec_name);
cout<<"Enter the Message: ";
getline(cin,mess);

//prepairing statement
check = sqlite3_prepare_v2(db,"INSERT INTO mess(sen_name,rec_name,mess)"
                            "VALUES (?,?,?);",-1,&stmt,nullptr);

 if(check!=SQLITE_OK){
    cerr<<"Error preparing statement!!!: "<<sqlite3_errmsg(db)<<endl;
    sqlite3_close(db);
return check;}

//binding the values into the statement
sqlite3_bind_text(stmt,1,sen_name.c_str(),-1,SQLITE_TRANSIENT);
sqlite3_bind_text(stmt,2,rec_name.c_str(),-1,SQLITE_TRANSIENT);
sqlite3_bind_text(stmt,3,mess.c_str(),-1,SQLITE_TRANSIENT);

check = sqlite3_step(stmt);
if(check!=SQLITE_DONE){
    cerr<<"Error inserting message!!!: "<<sqlite3_errmsg(db)<<endl;
    sqlite3_close(db);
return check;}
else{
    cout<<"Message inserted successfully!!!"<<endl;
}

// reading the database
check = sqlite3_prepare_v2(db,"SELECT * FROM mess;"
                            ,-1,&stmt,nullptr);

 if(check!=SQLITE_OK){
    cerr<<"Error preparing statement!!!: "<<sqlite3_errmsg(db)<<endl;
    sqlite3_close(db);
return check;}

while(sqlite3_step(stmt)==SQLITE_ROW){
 int id = sqlite3_column_int(stmt,0);
 const unsigned char* sen = sqlite3_column_text(stmt,1);
 const unsigned char* rec = sqlite3_column_text(stmt,2);
 const unsigned char* mess = sqlite3_column_text(stmt,3);
 const unsigned char* time = sqlite3_column_text(stmt,4);
cout<<"id: "<<id<<"\nSender: "<<sen<<"\nReciever: "<<rec<<"\nMessage: "<<mess<<"\nTime: "<<time<<endl;
}

//cleanup
sqlite3_finalize(stmt);
sqlite3_close(db);
return 0;}
