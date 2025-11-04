#include <bits/stdc++.h>
using namespace std;

// Utility: trim spaces
static inline string trim(const string &s){ size_t i=0,j=s.size(); while(i<j && isspace((unsigned char)s[i])) ++i; while(j>i && isspace((unsigned char)s[j-1])) --j; return s.substr(i,j-i);} 

// Tokenize respecting quotes
static vector<string> tokenize(const string &line){
    vector<string> t; string cur; bool inq=false; for(size_t i=0;i<line.size();++i){ char c=line[i];
        if(c=='"'){ inq=!inq; cur.push_back(c); }
        else if(!inq && isspace((unsigned char)c)) { if(!cur.empty()){ t.push_back(cur); cur.clear(); } }
        else cur.push_back(c);
    }
    if(!cur.empty()) t.push_back(cur);
    return t;
}

static bool isAsciiVisible(char c){ return c>=32 && c<=126; }
static bool isValidIDLike(const string &s){ if(s.empty()||s.size()>30) return false; for(char c: s){ if(!(isalnum((unsigned char)c) || c=='_')) return false; } return true; }
static bool isValidNameField(const string &s, size_t maxlen, bool allow_quote=false){ if(s.size()>maxlen) return false; for(char c: s){ if(!isAsciiVisible(c)) return false; if(!allow_quote && c=='"') return false; } return true; }

static bool parseInt(const string &s, long long &v){ if(s.empty()) return false; if(s.size()>10) return false; long long x=0; for(char c: s){ if(!isdigit((unsigned char)c)) return false; x = x*10 + (c-'0'); if(x>2147483647LL) return false; } v=x; return true; }

static bool parseMoney(const string &s, long long &cents){ if(s.empty()) return false; long long sign=1; size_t i=0; if(s[0]=='+'){sign=1;i++;} else if(s[0]=='-'){sign=-1;i++;}
    long long intpart=0; long long frac=0; int fraclen=-1; bool hasdot=false; if(i>=s.size()) return false; for(; i<s.size(); ++i){ char c=s[i]; if(c=='.'){ if(hasdot) return false; hasdot=true; fraclen=0; }
        else if(isdigit((unsigned char)c)){
            if(!hasdot){ intpart = intpart*10 + (c-'0'); if(intpart> (long long)9e12) return false; }
            else { if(fraclen<2){ frac = frac*10 + (c-'0'); fraclen++; } else { // allow more digits but ignore beyond 2? Spec fixed 2
                    return false; }
            }
        } else return false; }
    if(!hasdot){ cents = sign*intpart*100; return true; }
    if(fraclen==-1) return false; // dot without digits
    if(fraclen==0) frac*=10, fraclen++; // e.g., .1 treated as 10? Actually need exactly two decimals; enforce exactly two
    if(fraclen==1) frac*=10; // ensure two decimals
    cents = sign*(intpart*100 + frac);
    return true; }

struct User{ string id,password,username; int privilege; };
struct Book{ string isbn,name,author; vector<string> keywords; long long price_cents=0; long long stock=0; };

struct Frame{ string userid; string selected; };

static const char* USERS_DB = "users.db";
static const char* BOOKS_DB = "books.db";
static const char* TRANS_DB = "trans.db";

static void load_users(unordered_map<string,User>& users){
    ifstream fin(USERS_DB); if(!fin) return; string id,pwd,uname; int priv; string line;
    while(getline(fin,line)){
        if(line.empty()) continue; stringstream ss(line); string privs; if(!getline(ss,id,'\t')) continue; if(!getline(ss,pwd,'\t')) continue; if(!getline(ss,privs,'\t')) continue; if(!getline(ss,uname)) uname=""; try{ priv=stoi(privs);}catch(...){continue;} users[id]=User{id,pwd,uname,priv};
    }
}
static void save_users(const unordered_map<string,User>& users){ ofstream fout(USERS_DB, ios::trunc); for(auto &p: users){ const auto &u=p.second; fout<<u.id<<'\t'<<u.password<<'\t'<<u.privilege<<'\t'<<u.username<<"\n"; } }

static string join_keywords(const vector<string>& ks){ string s; for(size_t i=0;i<ks.size();++i){ if(i) s.push_back('|'); s+=ks[i]; } return s; }
static vector<string> split_keywords(const string& s){ vector<string> ks; if(s.empty()) return ks; string cur; for(char c: s){ if(c=='|'){ ks.push_back(cur); cur.clear(); } else cur.push_back(c);} ks.push_back(cur); return ks; }

static void load_books(unordered_map<string,Book>& books){ ifstream fin(BOOKS_DB); if(!fin) return; string line; while(getline(fin,line)){
        if(line.empty()) continue; stringstream ss(line); string isbn,name,author,kw,price,stock; if(!getline(ss,isbn,'\t')) continue; getline(ss,name,'\t'); getline(ss,author,'\t'); getline(ss,kw,'\t'); getline(ss,price,'\t'); getline(ss,stock,'\t'); if(price.empty()) getline(ss,price); if(stock.empty()) getline(ss,stock);
        Book b; b.isbn=isbn; b.name=name; b.author=author; b.keywords=split_keywords(kw); try{ b.price_cents=stoll(price);}catch(...){b.price_cents=0;} try{ b.stock=stoll(stock);}catch(...){b.stock=0;} books[isbn]=b;
    } }
static void save_books(const unordered_map<string,Book>& books){ ofstream fout(BOOKS_DB, ios::trunc); for(auto &p: books){ const auto &b=p.second; fout<<b.isbn<<'\t'<<b.name<<'\t'<<b.author<<'\t'<<join_keywords(b.keywords)<<'\t'<<b.price_cents<<'\t'<<b.stock<<"\n"; } }

static void load_transactions(vector<long long>& t){ ifstream fin(TRANS_DB); if(!fin) return; long long v; while(fin>>v){ t.push_back(v);} }
static void save_transactions(const vector<long long>& t){ ofstream fout(TRANS_DB, ios::trunc); for(long long v: t) fout<<v<<"\n"; }

int main(){ ios::sync_with_stdio(false); cin.tie(nullptr);
    unordered_map<string, User> users; users.reserve(10007);
    unordered_map<string, Book> books; books.reserve(10007);
    vector<Frame> stack;
    vector<long long> transactions; // positive income, negative expense

    load_users(users);
    load_books(books);
    load_transactions(transactions);

    auto current_priv = [&](){ if(stack.empty()) return 0; return users[stack.back().userid].privilege; };

    auto ensure_root = [&](){ if(users.find("root")==users.end()){ users["root"] = User{"root","sjtu","root",7}; save_users(users);} };
    ensure_root();

    string line;
    while(true){ string raw; if(!getline(cin, raw)) break; line = trim(raw); bool onlyspace = true; for(char c: raw){ if(!isspace((unsigned char)c)){ onlyspace=false; break; } } if(onlyspace){ continue; }
        auto tokens = tokenize(line);
        if(tokens.empty()) continue;
        string cmd = tokens[0];
        auto invalid = [&](){ cout << "Invalid\n"; };
        if(cmd=="quit" || cmd=="exit"){ break; }
        else if(cmd=="su"){
            // su [UserID] ([Password])?
            if(tokens.size()<2 || tokens.size()>3){ invalid(); continue; }
            string uid = tokens[1]; if(!isValidIDLike(uid)){ invalid(); continue; }
            ensure_root(); auto it = users.find(uid); if(it==users.end()){ invalid(); continue; }
            bool ok=false; if(tokens.size()==3){ ok = (it->second.password==tokens[2]); }
            else { int curp = current_priv(); if(curp > it->second.privilege) ok=true; }
            if(!ok){ invalid(); continue; }
            stack.push_back(Frame{uid, string()});
        }
        else if(cmd=="logout"){
            if(tokens.size()!=1){ invalid(); continue; }
            if(stack.empty()){ invalid(); continue; }
            stack.pop_back();
        }
        else if(cmd=="register"){
            if(tokens.size()!=4){ invalid(); continue; }
            string uid=tokens[1], pwd=tokens[2], uname=tokens[3];
            if(!isValidIDLike(uid) || !isValidIDLike(pwd) || !isValidNameField(uname,30,true)){ invalid(); continue; }
            if(users.count(uid)){ invalid(); continue; }
            users[uid] = User{uid,pwd,uname,1};
            save_users(users);
        }
        else if(cmd=="passwd"){
            if(tokens.size()!=3 && tokens.size()!=4){ invalid(); continue; }
            if(current_priv()<1){ invalid(); continue; }
            string uid=tokens[1]; if(!isValidIDLike(uid)){ invalid(); continue; }
            auto it=users.find(uid); if(it==users.end()){ invalid(); continue; }
            if(current_priv()==7){ string newp = (tokens.size()==3? tokens[2]: tokens[3]); if(!isValidIDLike(newp)){ invalid(); continue; } it->second.password=newp; }
            else {
                if(tokens.size()!=4){ invalid(); continue; }
                string cur=tokens[2], nw=tokens[3]; if(!isValidIDLike(cur) || !isValidIDLike(nw)){ invalid(); continue; }
                if(it->second.password!=cur){ invalid(); continue; }
                it->second.password=nw;
            }
            save_users(users);
        }
        else if(cmd=="useradd"){
            if(tokens.size()!=5){ invalid(); continue; }
            if(current_priv()<3){ invalid(); continue; }
            string uid=tokens[1], pwd=tokens[2], privs=tokens[3], uname=tokens[4];
            if(!isValidIDLike(uid) || !isValidIDLike(pwd) || !isValidNameField(uname,30,true)) { invalid(); continue; }
            long long pv; if(!parseInt(privs,pv)) { invalid(); continue; }
            if(!(pv==1 || pv==3 || pv==7)){ invalid(); continue; }
            if(pv>=current_priv()){ invalid(); continue; }
            if(users.count(uid)){ invalid(); continue; }
            users[uid]=User{uid,pwd,uname,(int)pv};
            save_users(users);
        }
        else if(cmd=="delete"){
            if(tokens.size()!=2){ invalid(); continue; }
            if(current_priv()!=7){ invalid(); continue; }
            string uid=tokens[1]; if(!isValidIDLike(uid)){ invalid(); continue; }
            if(!users.count(uid)){ invalid(); continue; }
            bool logged=false; for(auto &f: stack){ if(f.userid==uid){ logged=true; break; } }
            if(logged){ invalid(); continue; }
            users.erase(uid);
            save_users(users);
        }
        else if(cmd=="select"){
            if(tokens.size()!=2){ invalid(); continue; }
            if(current_priv()<3){ invalid(); continue; }
            string isbn=tokens[1]; if(isbn.size()>20){ invalid(); continue; }
            for(char c: isbn){ if(!isAsciiVisible(c)) { invalid(); goto nextline; } }
            {
                auto it=books.find(isbn);
                if(it==books.end()){
                    Book b; b.isbn=isbn; books.emplace(isbn, b);
                }
                if(!stack.empty()) stack.back().selected=isbn; else { invalid(); }
            }
        }
        else if(cmd=="modify"){
            if(current_priv()<3){ invalid(); continue; }
            if(stack.empty() || stack.back().selected.empty()){ invalid(); continue; }
            // parse options: one or more
            if(tokens.size()<2){ invalid(); continue; }
            bool has_isbn=false, has_name=false, has_author=false, has_keyword=false, has_price=false; 
            string new_isbn, new_name, new_author, new_keyword; long long new_price=LLONG_MIN;
            for(size_t i=1;i<tokens.size();++i){ string opt=tokens[i]; if(opt.rfind("-ISBN=",0)==0){ if(has_isbn){ invalid(); goto nextline; } has_isbn=true; new_isbn=opt.substr(6); if(new_isbn.empty()||new_isbn.size()>20){ invalid(); goto nextline; } for(char c: new_isbn){ if(!isAsciiVisible(c)){ invalid(); goto nextline; } } }
                else if(opt.rfind("-name=",0)==0){ if(has_name){ invalid(); goto nextline; } has_name=true; string v=opt.substr(6); if(v.size()<2||v.front()!='"'||v.back()!='"'){ invalid(); goto nextline; } v=v.substr(1,v.size()-2); if(!isValidNameField(v,60,false)){ invalid(); goto nextline; } new_name=v; }
                else if(opt.rfind("-author=",0)==0){ if(has_author){ invalid(); goto nextline; } has_author=true; string v=opt.substr(8); if(v.size()<2||v.front()!='"'||v.back()!='"'){ invalid(); goto nextline; } v=v.substr(1,v.size()-2); if(!isValidNameField(v,60,false)){ invalid(); goto nextline; } new_author=v; }
                else if(opt.rfind("-keyword=",0)==0){ if(has_keyword){ invalid(); goto nextline; } has_keyword=true; string v=opt.substr(9); if(v.size()<2||v.front()!='"'||v.back()!='"'){ invalid(); goto nextline; } v=v.substr(1,v.size()-2); if(!isValidNameField(v,60,false)){ invalid(); goto nextline; } // parse keywords
                        if(!v.empty()){
                            vector<string> segs; string cur; set<string> seen; bool dup=false; for(char c: v){ if(c=='|'){ if(cur.empty()){ dup=true; break; } if(seen.count(cur)){ dup=true; break; } seen.insert(cur); segs.push_back(cur); cur.clear(); } else cur.push_back(c); }
                                if(dup || cur.empty() || seen.count(cur)){ invalid(); goto nextline; }
                                segs.push_back(cur);
                        }
                        new_keyword=v;
                }
                else if(opt.rfind("-price=",0)==0){ if(has_price){ invalid(); goto nextline; } has_price=true; string v=opt.substr(7); long long cts; if(!parseMoney(v,cts)) { invalid(); goto nextline; } if(cts<0){ invalid(); goto nextline; } new_price=cts; }
                else { invalid(); goto nextline; }
            }
            {
                // apply changes with rules
                auto &sel_isbn = stack.back().selected; auto it = books.find(sel_isbn); if(it==books.end()){ invalid(); goto nextline; }
                Book b = it->second; // copy
                if(has_isbn){ if(new_isbn==b.isbn){ invalid(); goto nextline; } if(books.count(new_isbn)){ invalid(); goto nextline; } }
                if(has_keyword){ // check duplicates already done
                }
                // commit
                if(has_isbn){ books.erase(it); b.isbn=new_isbn; books.emplace(b.isbn,b); stack.back().selected=b.isbn; }
                else {
                    if(has_name) b.name=new_name; if(has_author) b.author=new_author; if(has_keyword){ // assign
                        b.keywords.clear(); if(!new_keyword.empty()){ string cur; for(char c: new_keyword){ if(c=='|'){ b.keywords.push_back(cur); cur.clear(); } else cur.push_back(c); } b.keywords.push_back(cur); }
                    }
                    if(has_price) b.price_cents=new_price; books[b.isbn]=b;
                }
                save_books(books);
            }
        }
        else if(cmd=="import"){
            if(tokens.size()!=3){ invalid(); continue; }
            if(current_priv()<3){ invalid(); continue; }
            if(stack.empty() || stack.back().selected.empty()){ invalid(); continue; }
            long long qty; if(!parseInt(tokens[1], qty) || qty<=0){ invalid(); continue; }
            long long cents; if(!parseMoney(tokens[2], cents) || cents<=0){ invalid(); continue; }
            auto it=books.find(stack.back().selected); if(it==books.end()){ invalid(); continue; }
            it->second.stock += qty; books[it->first]=it->second; transactions.push_back(-cents);
            save_books(books); save_transactions(transactions);
        }
        else if(cmd=="buy"){
            if(tokens.size()!=3){ invalid(); continue; }
            if(current_priv()<1){ invalid(); continue; }
            string isbn=tokens[1]; auto it=books.find(isbn); if(it==books.end()){ invalid(); continue; }
            long long qty; if(!parseInt(tokens[2], qty) || qty<=0){ invalid(); continue; }
            if(it->second.stock < qty){ invalid(); continue; }
            long long total = it->second.price_cents * qty; it->second.stock -= qty; books[isbn]=it->second; transactions.push_back(total);
            save_books(books); save_transactions(transactions);
            // output total with 2 decimals
            cout.setf(std::ios::fixed); cout<<setprecision(2)<< (total/100) <<'.'<< setw(2) << setfill('0') << (llabs(total)%100) <<"\n"; cout<<setfill(' ');
        }
        else if(cmd=="show"){
            // handle finance first
            if(tokens.size()>=2 && tokens[1]=="finance"){
                if(tokens.size()>3){ invalid(); continue; }
                if(current_priv()!=7){ invalid(); continue; }
                if(tokens.size()==2){ long long inc=0, exp=0; for(long long v: transactions){ if(v>=0) inc+=v; else exp+=-v; } cout.setf(std::ios::fixed); cout<<"+ "<<setprecision(2)<<(inc/100)<<'.'<<setw(2)<<setfill('0')<<(llabs(inc)%100)<<" - "<<(exp/100)<<'.'<<setw(2)<<setfill('0')<<(llabs(exp)%100)<<"\n"; cout<<setfill(' '); }
                else { long long cnt; if(!parseInt(tokens[2],cnt)) { invalid(); continue; } if(cnt==0){ cout<<"\n"; continue; } if(cnt>(long long)transactions.size()){ invalid(); continue; } long long inc=0, exp=0; for(long long i=transactions.size()-cnt;i<(long long)transactions.size();++i){ long long v=transactions[i]; if(v>=0) inc+=v; else exp+=-v; } cout.setf(std::ios::fixed); cout<<"+ "<<setprecision(2)<<(inc/100)<<'.'<<setw(2)<<setfill('0')<<(llabs(inc)%100)<<" - "<<(exp/100)<<'.'<<setw(2)<<setfill('0')<<(llabs(exp)%100)<<"\n"; cout<<setfill(' '); }
                continue;
            }
            if(current_priv()<1){ invalid(); continue; }
            // options: 0 or 1
            string mode; string val; if(tokens.size()==1){ mode="all"; }
            else if(tokens.size()==2){ string opt=tokens[1]; if(opt.rfind("-ISBN=",0)==0){ mode="ISBN"; val=opt.substr(6); if(val.empty()){ invalid(); continue; } }
                else if(opt.rfind("-name=",0)==0){ mode="name"; val=opt.substr(6); if(val.size()<2||val.front()!='"'||val.back()!='"'){ invalid(); continue; } val=val.substr(1,val.size()-2); if(val.empty()){ invalid(); continue; } }
                else if(opt.rfind("-author=",0)==0){ mode="author"; val=opt.substr(8); if(val.size()<2||val.front()!='"'||val.back()!='"'){ invalid(); continue; } val=val.substr(1,val.size()-2); if(val.empty()){ invalid(); continue; } }
                else if(opt.rfind("-keyword=",0)==0){ mode="keyword"; val=opt.substr(9); if(val.size()<2||val.front()!='"'||val.back()!='"'){ invalid(); continue; } val=val.substr(1,val.size()-2); if(val.empty()){ invalid(); continue; } if(val.find('|')!=string::npos){ invalid(); continue; } }
                else { invalid(); continue; } }
            else { invalid(); continue; }
            // collect matches
            vector<Book> out; out.reserve(books.size()); for(auto &p: books){ const Book &b=p.second; bool ok=false; if(mode=="all") ok=true; else if(mode=="ISBN") ok=(b.isbn==val); else if(mode=="name") ok=(b.name==val); else if(mode=="author") ok=(b.author==val); else if(mode=="keyword"){ for(auto &k: b.keywords){ if(k==val){ ok=true; break; } } }
                if(ok) out.push_back(b);
            }
            sort(out.begin(), out.end(), [](const Book &a, const Book &b){ return a.isbn < b.isbn; });
            if(out.empty()){ cout<<"\n"; }
            else{
                for(size_t i=0;i<out.size();++i){ const Book &b=out[i]; cout<<b.isbn<<'\t'<<(b.name)<<'\t'<<(b.author)<<'\t';
                    // keyword join
                    for(size_t j=0;j<b.keywords.size();++j){ if(j) cout<<'|'; cout<<b.keywords[j]; }
                    // price
                    long long pc=b.price_cents; cout<<'\t'; cout.setf(std::ios::fixed); cout<<setprecision(2)<<(pc/100)<<'.'<<setw(2)<<setfill('0')<<(llabs(pc)%100); cout<<setfill(' ');
                    cout<<'\t'<<b.stock<<"\n";
                }
            }
        }
        else if(cmd=="show" && tokens.size()>1 && tokens[1]=="finance"){ // dead branch; handled below
            invalid();
        }
        else if(cmd=="log"){
            if(current_priv()!=7){ invalid(); continue; }
            // self-defined, keep empty output
            cout<<""; 
        }
        else if(cmd=="report"){
            if(tokens.size()!=2){ invalid(); continue; }
            if(current_priv()!=7){ invalid(); continue; }
            // accept both finance and employee, output nothing
            if(tokens[1]=="finance" || tokens[1]=="employee"){ /* no output */ }
            else { invalid(); }
        }
        else if(cmd=="show" && tokens.size()>=2 && tokens[1]=="finance"){
            if(tokens.size()>3){ invalid(); continue; }
            if(current_priv()!=7){ invalid(); continue; }
            if(tokens.size()==2){ long long inc=0, exp=0; for(long long v: transactions){ if(v>=0) inc+=v; else exp+=-v; } cout.setf(std::ios::fixed); cout<<"+ "<<setprecision(2)<<(inc/100)<<'.'<<setw(2)<<setfill('0')<<(llabs(inc)%100)<<" - "<<(exp/100)<<'.'<<setw(2)<<setfill('0')<<(llabs(exp)%100)<<"\n"; cout<<setfill(' ');
            } else { long long cnt; if(!parseInt(tokens[2],cnt)) { invalid(); continue; } if(cnt==0){ cout<<"\n"; continue; } if(cnt>(long long)transactions.size()){ invalid(); continue; } long long inc=0, exp=0; for(long long i=transactions.size()-cnt;i<(long long)transactions.size();++i){ long long v=transactions[i]; if(v>=0) inc+=v; else exp+=-v; } cout.setf(std::ios::fixed); cout<<"+ "<<setprecision(2)<<(inc/100)<<'.'<<setw(2)<<setfill('0')<<(llabs(inc)%100)<<" - "<<(exp/100)<<'.'<<setw(2)<<setfill('0')<<(llabs(exp)%100)<<"\n"; cout<<setfill(' ');
            }
        }
        else {
            invalid();
        }
        nextline: ;
    }
    return 0;
}
