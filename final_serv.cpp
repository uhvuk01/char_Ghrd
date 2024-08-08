#include <iostream>
#include <mariadb/conncpp.hpp>
#include <cstdlib>
#include <unistd.h>
#include <string>
#include <cstring>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <pthread.h>
#include <algorithm>
#include <map>
// #include <nlohmann/json.hpp>
#include "json.hpp"
using json = nlohmann::json; // nlohmann::json를 json으로 간소화

#define BUF_SIZE 100
#define MAX_CLNT 256
#define NAME_SIZE 20

void *backgroundthread(void *arg);
void error_handling(const char *msg);
void login_logic(int clnt_sock);
int clnt_cnt = 0;         // 소켓 디스크립터의 인덱스 번호 역할
int clnt_socks[MAX_CLNT]; // 소켓 디스크립터 담을 256개

std::vector<int> chat_CS_socks;
std::vector<int> chat_CC_socks;

pthread_mutex_t mutx; // 뮤텍스 변수

class DB
{
public:
    sql::Connection *conn;
    DB() {}
    void connect()
    {
        try
        {
            sql::Driver *driver = sql::mariadb::get_driver_instance();
            sql::SQLString url = ("jdbc:mariadb://10.10.21.118:3306/PJ_CHAT_GHRD");
            sql::Properties properties({{"user", "Student"}, {"password", "1234"}});
            conn = driver->connect(url, properties);
        }
        catch (sql::SQLException &e)
        {
            std::cerr << "Error Connecting to MariaDB Platform: " << e.what() << std::endl;
        }
    }

    ~DB() { delete conn; }
};
void remove_clnt_serv(int clnt_sock, DB &database);
//////////////////
class Login{
    DB& DB_login;
public:    
    Login(DB& DB_init): DB_login(DB_init){}
    int CC_Login_logic(int clnt_sock,char login_pw[][BUF_SIZE]){
        char success_msg[BUF_SIZE] = "로그인 되었습니다.\n";
        char deny_msg[BUF_SIZE] = "일치하는 정보가 없습니다.\n";
        std::string input_ID = login_pw[0];
        std::string output_ID ;
        std::string input_PW = login_pw[1];
        std::string output_PW;
        while(1){
            std::cout<<"로그인로직\n";
            std::cout<<"받아온아이디 = "<<login_pw[0]<<std::endl;
            std::cout<<"받아온비번 = "<<login_pw[1]<<std::endl;
            try{
                std::cout<<"aa"<<std::endl;
                std::unique_ptr <sql::Statement > con(DB_login.conn -> createStatement()); //객체 생성
                sql::ResultSet *res =con->executeQuery("SELECT ID, PW FROM CC_INFO WHERE ID = '" + input_ID + "'AND PW = '" + input_PW + "'");
                while (res->next())
                {
                    output_ID = res->getString(1);
                    output_PW = res->getString(2);
                }
                if(input_ID == output_ID && input_PW == output_PW){
                    update_discripter(output_ID, clnt_sock,0);
                    write(clnt_sock,success_msg,strlen(success_msg)); //로그인 성공 메세지 전달
                    std::cout<<"success ="<<output_ID<<std::endl;
                    std::cout<<"success ="<<output_PW<<std::endl;
                    return 0;
                    // sql::ResultSetres =under_5->executeQuery("UPDATE USER_INFO SET LOG = 1 WHERE ID = '" + input_ID + "'");
                }
                else{
                    write(clnt_sock,deny_msg,strlen(deny_msg)); // 로그인 실패 메세지 전달
                    std::cout<<"fail ="<<output_ID<<std::endl;
                    std::cout<<"fail ="<<output_PW<<std::endl;
                    return -1;
                }
                // write(clnt_sock,output_ID.data(),strlen(output_ID.data()));
            }
            catch(sql::SQLException& e){
                std::cerr << "Error Connecting to MariaDB Platform: " << e.what() << std::endl;
                return -1;
            }
        }
    }
    int CS_Login_logic(int clnt_sock, char login_pw[][BUF_SIZE])
    {
        char success_msg[BUF_SIZE] = "로그인 되었습니다.\n";
        char deny_msg[BUF_SIZE] = "일치하는 정보가 없습니다.\n";
        std::string input_ID = login_pw[0];
        std::string output_ID;
        std::string input_PW = login_pw[1];
        std::string output_PW;
        while (1)
        {
            std::cout << "로그인로직\n";
            std::cout << "받아온아이디 = " << login_pw[0] << std::endl;
            std::cout << "받아온비번 = " << login_pw[1] << std::endl;
            try
            {
                std::cout << "aa" << std::endl;
                std::unique_ptr<sql::Statement> con(DB_login.conn->createStatement()); // 객체 생성
                sql::ResultSet *res = con->executeQuery("SELECT ID, PW FROM CS_INFO WHERE ID = '" + input_ID + "'AND PW = '" + input_PW + "'");
                while (res->next())
                {
                    output_ID = res->getString(1);
                    output_PW = res->getString(2);
                }
                if (input_ID == output_ID && input_PW == output_PW)
                {                                                       // 로그인성공 조건통과
                    update_discripter(output_ID, clnt_sock,1);            // 디스크립터 업데이트 함수
                    write(clnt_sock, success_msg, strlen(success_msg)); // 로그인 성공 메세지 전달
                    std::cout << "success =" << output_ID << std::endl;
                    std::cout << "success =" << output_PW << std::endl;
                    return 0;
                    // sql::ResultSetres =under_5->executeQuery("UPDATE USER_INFO SET LOG = 1 WHERE ID = '" + input_ID + "'");
                }
                else
                {
                    write(clnt_sock, deny_msg, strlen(deny_msg)); // 로그인 실패 메세지 전달
                    std::cout << "fail =" << output_ID << std::endl;
                    std::cout << "fail =" << output_PW << std::endl;
                    return -1;
                }
                // write(clnt_sock,output_ID.data(),strlen(output_ID.data()));
            }
            catch (sql::SQLException &e)
            {
                std::cerr << "Error Connecting to MariaDB Platform: " << e.what() << std::endl;
                return -1;
            }
        }
    }
    void update_discripter(std::string ID, int clnt_sock, int cc_or_cs){
        if(cc_or_cs ==0){
            try {
                std::unique_ptr<sql::PreparedStatement> insert_info(DB_login.conn->prepareStatement("UPDATE CC_INFO SET DISCRIPTER = ? WHERE ID = ?"));
                insert_info->setInt(1, clnt_sock);//디스크립터
                insert_info->setString(2, ID);//아이디
                insert_info->executeQuery();
            }
            catch(sql::SQLException& e){
                std::cerr << "Error cc 디스크립터 업데이트 new task: " << e.what() << std::endl;
            }
        }
        else{
            try {
                std::unique_ptr<sql::PreparedStatement> insert_info(DB_login.conn->prepareStatement("UPDATE CS_INFO SET DISCRIPTER = ? WHERE ID = ?"));
                insert_info->setInt(1, clnt_sock);//디스크립터
                insert_info->setString(2, ID);//아이디
                insert_info->executeQuery();
            }
            catch(sql::SQLException& e){
                std::cerr << "Error cs 디스크립터 업데이트 new task: " << e.what() << std::endl;
            }
        }
    }
};
/////////////////
class Signup
{
    DB &DB_signup;

public:
    Signup(DB &DB_init) : DB_signup(DB_init) {}

    void Signup_logic(int clnt_sock, char user_info[][BUF_SIZE])
    {
        enum spectrum
        {
            NNAME = 0,
            ID = 1,
            PW = 2

        };
        std::map<std::string, std::string> id_pw_info;
        std::string input_values[5];
        char already_is[BUF_SIZE] = "존재하는 아이디입니다\n";
        int rand_num;
        int notice_rnum;
        std::string str_rnum;
        input_values[0] = user_info[NNAME];
        input_values[1] = user_info[ID];
        input_values[2] = user_info[PW];
        char notice[BUF_SIZE] = "회원가입을 축하합니다.\n";
        char head[BUF_SIZE] = "당신의 고유번호는 \n";
        char bottom[BUF_SIZE] = "입니다.\n";
        srand(time(NULL));
        std::cout << "아이디체크1" << std::endl;

        try
        {
            std::cout << "input_values" + input_values[0] << std::endl;
            std::cout << "input_values" + input_values[1] << std::endl;
            std::cout << "input_values" + input_values[2] << std::endl;

            std::cout << "아이디체크2" << std::endl;
            std::map<std::string, std::string>::iterator iter;
            std::string z, l;
            std::unique_ptr<sql::Statement> stmnt(DB_signup.conn->createStatement()); // 객체 생성
            sql::ResultSet *res = stmnt->executeQuery("SELECT ID,PW FROM CC_INFO WHERE ID ='" + input_values[0] + "'");
            while (res->next())
            {
                std::cout << "아이디체크3" << std::endl;

                z = res->getString(1);              // id
                l = res->getString(2);              // pw
                id_pw_info.insert(std::pair(z, l)); // 아이디 비번 맵에 넣기
                iter = id_pw_info.find(z);
                iter->first;  // id;
                iter->second; // pw
                std::cout << std::string(iter->first) << std::endl;
                std::cout << std::string(iter->second) << std::endl;
            }
            if (id_pw_info.find(input_values[0]) != id_pw_info.end())
            {
                std::cout << "아이디체크4" << std::endl;

                write(clnt_sock, already_is, strlen(already_is)); // 이미 있는 아이디
            }
            else
            { // 중복된 아이디 아니니까 여기로 (회원가입 데이터 삽입 )
                try
                {
                    std::cout << "아이디체크5" << std::endl;

                    std::unique_ptr<sql::PreparedStatement> insert_info(DB_signup.conn->prepareStatement("INSERT INTO CC_INFO"
                                                                                                         " values (default, ?, ?, ?, ?)"));
                    insert_info->setString(1, input_values[0]); // NAME
                    insert_info->setString(2, input_values[1]); // 아이디
                    insert_info->setString(3, input_values[2]); // 패스워드
                    insert_info->setInt(4, clnt_sock);          // 디스크립터 했네 굳

                    insert_info->executeQuery();
                }
                catch (sql::SQLException &e)
                {
                    std::cerr << "Error 회원등록 인서트 new task: " << e.what() << std::endl;
                    exit(1);
                }
                try
                {
                    std::unique_ptr<sql::Statement> stmnt(DB_signup.conn->createStatement());                                    // 객체 생성
                    sql::ResultSet *res = stmnt->executeQuery("SELECT CSNUM FROM CC_INFO WHERE ID = '" + input_values[0] + "'"); // 이거 왜 하는거지 몇번쨰 고객인지 알려주는거네
                    while (res->next())
                    {
                        notice_rnum = res->getInt(1);
                    }
                }
                catch (sql::SQLException &e)
                {
                    std::cerr << "Error 회원등록 인서트 new task: " << e.what() << std::endl;
                    exit(1);
                }
                str_rnum = std::__cxx11::to_string(notice_rnum - 1000);       // rnum 스트링으로 형변환 고유번호
                write(clnt_sock, notice, strlen(notice));                     // 회원가입을 축하합니다.
                write(clnt_sock, head, strlen(head));                         // 당신의 고유번호는
                write(clnt_sock, str_rnum.c_str(), strlen(str_rnum.c_str())); // 고유번호
                write(clnt_sock, bottom, strlen(bottom));                     // 입니다.
            }
        }
        catch (sql::SQLException &e)
        {
            std::cout << "Error inserting new task: " << e.what() << std::endl;
            exit(1);
        }
    }
};
/////////////////
class Chat_CS_CC
{
    DB &DB_Chat_CS_CC;

public:
    Chat_CS_CC(DB &DB_init) : DB_Chat_CS_CC(DB_init) {}
    void user_data(int User_socknum, int Cs_socknum, std::string keyword)
    {
        // cc num csnum 상담 키워드 넣어야하네
        // 상담 내역 남기기
        int User_outdata;
        int Cs_outdata;
        std::string Cs_outdatastring;
        try
        {
            std::cout << "트라이 진입 User_socknum: " << User_socknum << std::endl;
            std::cout << "트라이 진입 Cs_socknum: " << Cs_socknum << std::endl;

            // 소켓 넘버를 두개다 가져와서 그걸로 각각 찾고 고객번호 상담사 번호를 찾아서 그걸 담아서 다시 값을 넣어야겠네
            // 고객 번호 찾기
            std::cout << "1번" << std::endl;

            sql::PreparedStatement *stmnt3 =
                DB_Chat_CS_CC.conn->prepareStatement("SELECT CCNUM FROM CC_INFO WHERE DISCRIPTER = ?"); // 고객
            stmnt3->setInt(1, User_socknum);
            sql::ResultSet *res = stmnt3->executeQuery();
            while (res->next())
            {
                User_outdata = res->getInt(1); // 고객번호
            }
            std::cout << "2번" << std::endl;

            std::cout << "첫번째 트라이 끝 User_outdata: " << User_outdata << std::endl;
        }
        catch (sql::SQLException &e)
        {
            std::cerr << "Error 상담기록 인서트 new task: " << e.what() << std::endl;
            // return -1;
            exit(1);
        }

        try
        {
            std::cout<<"두번째 try 진입 Cs_socknum = "<<Cs_socknum<<std::endl;
            std::unique_ptr<sql::Statement> stmnt(DB_Chat_CS_CC.conn->createStatement());                                    // 객체 생성
            sql::ResultSet *res = stmnt->executeQuery("SELECT CSNUM FROM CS_INFO WHERE DISCRIPTER = "+ std::__cxx11::to_string(Cs_socknum) +""); // 이거 왜 하는거지 몇번쨰 고객인지 알려주는거네
            while (res->next())
            {
                Cs_outdata = res->getInt(1); // 상담사 번호
            }
        }
        catch (sql::SQLException &e)
        {
            std::cerr << "Error 회원등록 인서트 new task: " << e.what() << std::endl;
            exit(1);
        }

        std::cout << "두번째 트라이 나오고 나서 Cs_outdata: " << Cs_outdata << std::endl;

        try{
            std::cout << "세번째 트라이 진입 Cs_outdata: " << Cs_outdata << std::endl;
             std::unique_ptr<sql::PreparedStatement> insert_info(DB_Chat_CS_CC.conn->prepareStatement("INSERT INTO SERVICE_HISTORY values (default, ?, ?, ?, default)"));

            insert_info->setInt(1, Cs_outdata);   // 유저번호Cs_outdata
            insert_info->setInt(2, User_outdata); // 상담사 번호User_outdata
            insert_info->setString(3, keyword);   // 키워드
            std::cout << "4번" << std::endl;

            insert_info->executeQuery();
            std::cout << "5번" << std::endl;
            // return 1;

        }
         catch (sql::SQLException &e)
        {
            std::cerr << "Error 인서트 new task: " << e.what() << std::endl;
            // return -1;
            exit(1);
        }
    }
    void chat_CC_wait(int CC_sock)
    { // 고객이 대기
        int CC_CS[2];
        char msg[BUF_SIZE];
        char end_msg[BUF_SIZE] = "연결이 종료되었습니다.\n";
        std::cout << "chat_CC_wait 입장" << std::endl;
        chat_CC_socks.emplace_back(CC_sock); // push 랑 같음
        while (1)
        {
            sleep(1);
            if (chat_CC_socks.size() >= 1 && chat_CS_socks.size() >= 1)
            {
                write(CC_sock, "채팅가능", strlen("채팅가능"));
                CC_CS[0] = chat_CC_socks[0];
                CC_CS[1] = chat_CS_socks[0];
                break;
            }
        }
        while (1)
        { // 대화하는 와일문
            memset(msg, 0, sizeof(msg));
            if (read(CC_CS[0], msg, sizeof(msg)) > 0)
            {
                std::cout << msg << std::endl;
                for (int i = 0; i < 2; i++)
                {
                    write(CC_CS[i], msg, strlen(msg));
                }
            }
            else
            {
                for (int i = 0; i < 2; i++)
                    write(CC_CS[i], end_msg, strlen(end_msg));
                break;
            }
        }
    }

    void chat_CS_wait(int CS_sock)
    { // 상담원이 대기
        std::string CCNUM;
        std::string NAME;
        std::string ID;
        int CC_CS[2];
        char msg[BUF_SIZE];
        char end_msg[BUF_SIZE] = "연결이 종료되었습니다.\n";
        chat_CS_socks.emplace_back(CS_sock); // push 랑 같음
        while (1)
        {
            sleep(1);
            if (chat_CC_socks.size() >= 1 && chat_CS_socks.size() >= 1)
            {
                CC_CS[0] = chat_CC_socks[0];
                CC_CS[1] = chat_CS_socks[0];
                std::cout <<"chat_CC_socks"<< chat_CC_socks[0]<<std::endl;
                std::cout <<"chat_CS_socks"<< chat_CS_socks[0]<<std::endl;
                try
                {
                    std::unique_ptr<sql::Statement> con(DB_Chat_CS_CC.conn->createStatement()); // 객체 생성
                    sql::ResultSet *res = con->executeQuery("SELECT CCNUM, NAME, ID FROM CC_INFO WHERE DISCRIPTER = " + std::__cxx11::to_string(CC_CS[0]));
                    while (res->next())
                    {
                        CCNUM = std::__cxx11::to_string(res->getInt(1));
                        NAME = res->getString(2);
                        ID = res->getString(3);
                    }
                }
                catch (sql::SQLException &e)
                {
                    std::cerr << "Error Connecting to MariaDB Platform: " << e.what() << std::endl;
                }
                write(CS_sock, CCNUM.c_str(), strlen(CCNUM.c_str()));
                usleep(100000);
                write(CS_sock, NAME.c_str(), strlen(NAME.c_str()));
                usleep(100000);
                write(CS_sock, ID.c_str(), strlen(ID.c_str()));
                usleep(100000);
                write(CS_sock, "채팅가능", strlen("채팅가능"));
                break;
            }
        }

        while (1)
        { // 대화하는 와일문
            memset(msg, 0, sizeof(msg));
            if (read(CC_CS[1], msg, sizeof(msg)) > 0)
            {
                if(strcmp(msg,"keyword_token")==0){
                    memset(msg, 0, sizeof(msg));
                    read(CC_CS[1], msg, sizeof(msg));
                    user_data(CC_CS[0], CC_CS[1], msg);
                }

                else{
                    std::cout << msg << std::endl;
                    for (int i = 0; i < 2; i++)
                    {
                        write(CC_CS[i], msg, strlen(msg));
                    }
                }

            }
            else
            {
                for (int i = 0; i < 2; i++)
                    write(CC_CS[i], end_msg, strlen(end_msg));
                break;
            }
        }
    }
};
int main(int argc, char *argv[])
{
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_adr, clnt_adr;
    socklen_t clnt_adr_sz;
    pthread_t t_id;

    pthread_mutex_init(&mutx, NULL);
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);

    if (argc != 2)
    {
        printf("Usage : %s <port>\n", argv[0]);
        exit(1);
    }
    memset(&serv_adr, 0, sizeof(serv_adr));
    serv_adr.sin_family = AF_INET;
    serv_adr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_adr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr *)&serv_adr, sizeof(serv_adr)) == -1)
        error_handling("bind() error");
    if (listen(serv_sock, 5) == -1)
        error_handling("listen() error");

    while (1)
    {
        try
        {
            clnt_adr_sz = sizeof(clnt_adr);
            clnt_sock = accept(serv_sock, (struct sockaddr *)&clnt_adr, &clnt_adr_sz);
            printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));
            pthread_mutex_lock(&mutx);
            clnt_socks[clnt_cnt++] = clnt_sock;
            pthread_mutex_unlock(&mutx);
            pthread_create(&t_id, NULL, backgroundthread, (void *)&clnt_sock);
            pthread_detach(t_id);
        }
        catch (std::exception &e)
        {
            std::cerr << "error : " << &e << std::endl;
        }
    }
    close(serv_sock);
    return 0;
}

void *backgroundthread(void *arg)
{
    DB DB_init;
    DB_init.connect();
    Login login_go(DB_init);
    Signup signup_go(DB_init);
    Chat_CS_CC cc_cs(DB_init);
    int clnt_sock = *((int *)arg);
    int login_flag = -2, cc_flag = -1, cs_flag = -1;
    char msg[BUF_SIZE];
    char plz_cs_msg[BUF_SIZE];
    char CC_login_pw[2][BUF_SIZE];
    char CS_login_pw[3][BUF_SIZE];
    char signup_pw[3][BUF_SIZE];

    while (1)
    {
        memset(msg, 0, sizeof(msg));
        if (read(clnt_sock, msg, sizeof(msg)) != 0) // 상담사 로그인 로직 (UI를 아이디, 비번, 상담사번호로 수정하셔야 합니다.)
        {
            std::cout << "로그인체크1" << std::endl;

            if (strcmp(msg, "_for_cs_login_") == 0)
            {
                std::cout << "로그인체크2" << std::endl;
                for (int i = 0; i < 2; i++)
                {
                    memset(CS_login_pw[i], 0, sizeof(CS_login_pw[i]));
                    if (read(clnt_sock, CS_login_pw[i], sizeof(CS_login_pw[i])) != 0)
                    {
                        std::cout << "CS_first_input = " << CS_login_pw[i] << std::endl;
                    }
                }
                if (login_flag = login_go.CS_Login_logic(clnt_sock, CS_login_pw) == 0)
                { // 주석된 부분 로그인 클래스의 CS로그인 전용 함수 만들어서 쓰시면 됩니다.

                    std::cout << "리턴값0받고나옴\n"; //
                    break;
                }
                else
                    std::cout << "리턴값-1받고나옴\n";
            }
            else if (strcmp(msg, "_for_abc_signup_") == 0) // 고객 회원가입 로직
            {
                std::cout << msg << std::endl;
                memset(msg, 0, sizeof(msg));
                std::cout << "signsignal" << std::endl;
                for (int i = 0; i < 3; i++)
                {
                    memset(signup_pw[i], 0, sizeof(signup_pw[i]));                // 메세지 (아이디 비밀번호 받기 전에 받을 배열 클리어해주기 )
                    if (read(clnt_sock, signup_pw[i], sizeof(signup_pw[i])) != 0) // 비어있는지 확인
                    {
                        std::cout << "clnt_first_input = " << signup_pw[i] << std::endl; // 클라이언트에서 회원가입 한걸 플래그 보내고 아이디 비번 이름 이렇게 3개를 보내게 해야겠다 클라이언트에서 총 4번 write해야겠다
                    }
                }
                signup_go.Signup_logic(clnt_sock, signup_pw);
            }
            else if (strcmp(msg, "_for_cc_login_") == 0)
            { // 고객 로그인 로직
                std::cout << msg << std::endl;
                for (int i = 0; i < 2; i++)
                {
                    memset(CC_login_pw[i], 0, sizeof(CC_login_pw[i]));
                    if (read(clnt_sock, CC_login_pw[i], sizeof(CC_login_pw[i])) != 0)
                    {
                        std::cout << "CC_first_input = " << CC_login_pw[i] << std::endl;
                    }
                }
                if (login_flag = login_go.CC_Login_logic(clnt_sock, CC_login_pw) == 0)
                {
                    std::cout << "리턴값0받고나옴\n"; // 이 멘트 대신 다음함수예정
                    login_flag = 0;
                    break;
                }
                else
                    std::cout << "리턴값-1받고나옴\n";
            }
        }
        else if (read(clnt_sock, msg, sizeof(msg)) < 0)
            remove_clnt_serv(clnt_sock, DB_init);
        else
        {
            std::cout << "이상한 메시지? = " << msg << std::endl;
            break;
        }
    }
    ////////////////////////////////////////////////////////////////
    if (login_flag == 0)
    {                                              // CC일때
        memset(plz_cs_msg, 0, sizeof(plz_cs_msg)); // CC로부터 상담요청메시지 받을 배열 초기화
        if (read(clnt_sock, plz_cs_msg, sizeof(plz_cs_msg)))
        {
            if (strcmp(plz_cs_msg, "_for_go_cs_") == 0)
            { // 이거랑 같으면
                std::cout << plz_cs_msg << std::endl;
                sleep(1);
                write(clnt_sock, plz_cs_msg, strlen(plz_cs_msg)); // 다시 보내줌 (서버와 클라이언트가 서로 같은 메시지를 받아서 다음 로직으로 넘어가도 되는지 검증)
                cc_cs.chat_CC_wait(clnt_sock);
            }
            else
            {
                std::cout << plz_cs_msg << std::endl;
                write(clnt_sock, plz_cs_msg, strlen(plz_cs_msg));
            }
        }
        else
            remove_clnt_serv(clnt_sock, DB_init);
    }

    else if (login_flag == 1)
    { // CS일때
        cc_cs.chat_CS_wait(clnt_sock);
    }
    return NULL;
}

void remove_clnt_serv(int clnt_sock, DB &database)
{
    pthread_mutex_lock(&mutx);
    try
    {
        std::unique_ptr<sql::Statement> stmnt(database.conn->createStatement()); // 객체 생성
        sql::ResultSet *res = stmnt->executeQuery("UPDATE CC_INFO SET DISCRIPTER = -1 WHERE DISCRIPTER = " + std::__cxx11::to_string(clnt_sock));
    }
    catch (sql::SQLException &e)
    {
        std::cerr << "Error 로그아웃 0 업데이트 task: " << e.what() << std::endl;
    }
    for (int i = 0; i < clnt_cnt; i++)
    {
        if (clnt_sock == clnt_socks[i])
        {
            while (i++ < clnt_cnt - 1)
                clnt_socks[i] = clnt_socks[i + 1];
            break;
        }
    }
    clnt_cnt--;
    pthread_mutex_unlock(&mutx);
    close(clnt_sock);
}
void error_handling(const char *msg)
{
    fputs(msg, stderr);
    std::cerr << '/n' << stderr;
    exit(1);
}