// C program to demonstrate use of fork() and pipe()
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<sys/wait.h>
#include<sys/socket.h>
#include "message.h"
#include "logging.h"
#include "limits.h"

#define PIPE(fd) socketpair(AF_UNIX, SOCK_STREAM, PF_UNIX, fd)

int initial_monster_count, status;
map_info info;
pid_t player_pid, monsters_pid[MONSTER_LIMIT];
int monsters_fd[MONSTER_LIMIT];
int fdp[2], fdm[MONSTER_LIMIT][2];
monster_message mm;

void closeAll(){
    waitpid(player_pid, &status,0);
    close(fdp[0]);
    mm.game_over=true;
    int fd_i;
    for(int i=0;i<info.alive_monster_count;i++){
        fd_i = monsters_fd[i];
        write(fdm[fd_i][0], &mm, sizeof(mm));
        
        waitpid(monsters_pid[i], &status, 0);
        close(fdm[fd_i][0]);
    }

}

void sortCoordinates(pid_t monsters_pid[], int monsters_fd[]){
    /* for(int i=0;i < info.alive_monster_count;i++){
        printf("when entered the sorted func----> x : %d, y: %d, fd : %d, pid =%d, type: %c \n ", info.monster_coordinates[i].x, info.monster_coordinates[i].y, monsters_fd[i], monsters_pid[i], info.monster_types[i]);
    } */
    
    char temp_type;
    pid_t temp_pid; 
    int temp_fd,temp_x,temp_y;

    //push all -1,-1 to the end
    int l=0, r=initial_monster_count-1;

    while(l<r){
        while(l<r && info.monster_coordinates[r].x==-1){
            r--;
        }
        if(info.monster_coordinates[l].x==-1){
            temp_type= info.monster_types[l];
            temp_pid= monsters_pid[l];
            temp_fd = monsters_fd[l];
            temp_x =info.monster_coordinates[l].x; 
            temp_y =info.monster_coordinates[l].y;

            info.monster_types[l]=info.monster_types[r];
            monsters_pid[l] = monsters_pid[r];
            monsters_fd[l]=monsters_fd[r];
            info.monster_coordinates[l].x=info.monster_coordinates[r].x;
            info.monster_coordinates[l].y=info.monster_coordinates[r].y;

            info.monster_types[r]=temp_type;
            monsters_pid[r] = temp_pid;
            monsters_fd[r]=temp_fd;
            info.monster_coordinates[r].x=temp_x;
            info.monster_coordinates[r].y=temp_y;
        }
        l++;
    }

    for (int i = 0; i < info.alive_monster_count; i++)      
    {
        
        for (int j = i + 1; j < info.alive_monster_count; j++)
        {
            if (info.monster_coordinates[i].x > info.monster_coordinates[j].x ||
             (info.monster_coordinates[i].x == info.monster_coordinates[j].x && info.monster_coordinates[i].y > info.monster_coordinates[j].y)) 
            {
                coordinate temp_cord =  info.monster_coordinates[i];
                temp_type= info.monster_types[i];
                temp_pid= monsters_pid[i];
                temp_fd = monsters_fd[i];

                info.monster_coordinates[i].x = info.monster_coordinates[j].x;
                info.monster_coordinates[i].y = info.monster_coordinates[j].y;
                info.monster_types[i] = info.monster_types[j];
                monsters_pid[i]=monsters_pid[j];
                monsters_fd[i]=monsters_fd[j];

                info.monster_coordinates[j].x = temp_cord.x;
                info.monster_coordinates[j].y = temp_cord.y;
                info.monster_types[j] = temp_type;
                monsters_pid[j] = temp_pid;
                monsters_fd[j] = temp_fd;
            }
        }
    }
    
}

bool isWall(coordinate *c){
    int width= info.map_width, height = info.map_height;
    if(c->x==0 || c->x==width-1 || c->y==0 || c->y==height-1){
        return true;
    }
    return false;

}
bool isDoor( coordinate *c){
    
    if(c->x==info.door.x && c->y==info.door.y){
        return true;
    }
    return false;

}
//check for only monsters
bool isOccupied(coordinate *c, int itself){
    //printf("isOccupied called for %d - %c\n",itself, info.monster_types[itself]);
    for(int i=0;i< info.alive_monster_count;i++){
        
        if(i!= itself && info.monster_coordinates[i].x==c->x && info.monster_coordinates[i].y==c->y){
            //printf("%c monster is here (%d, %d)\n", info.monster_types[i],info.monster_coordinates[i].x,info.monster_coordinates[i].y);
            return true;
        }
        
    }
    return false;

}

void gameOver(game_over_status go){
    print_map(&info);
    print_game_over(go);

}


int main()
{
    
    
    char s1[1024],s2[1024];
    PIPE(fdp);
    char playerPath[PATH_MAX];
    char p1[1024], p2[1024], p3[1024];
    char monsterPath[PATH_MAX];
    char mt;
    char* args[10];
    int l[7];
    
    scanf("%d %d", &info.map_width, &info.map_height);
    scanf("%d %d", &info.door.x, &info.door.y);
    
    scanf("%d %d", &info.player.x, &info.player.y);
    
    scanf("%s", playerPath);
    scanf("%s %s %s", p1, p2, p3);

    

    player_pid =fork();
    if(player_pid==0){ //player
        dup2(fdp[1], 0);
        dup2(fdp[1], 1);
        close(fdp[0]);
        close(fdp[1]);

        
        sprintf(s1, "%d", info.door.x);
        sprintf(s2, "%d", info.door.y);
        args[0] = playerPath;
        args[1]=s1;
        args[2]=s2;
        args[3]=p1;
        args[4]=p2;
        args[5]=p3;
        args[6] = NULL;

        
        execv(playerPath, args);


        
    }
    else{
        close(fdp[1]);
    }
    
   
    pid_t pid;
    
    
    char m3[1024],m4[1024], m5[1024],m6[1024];
    scanf("%d", &info.alive_monster_count);
    initial_monster_count=info.alive_monster_count;
    for(int i=0;i<info.alive_monster_count;i++){
        PIPE(fdm[i]);
        scanf("%s ", monsterPath);
        scanf("%c", &mt );
        scanf("%d %d %s %s %s %s", &info.monster_coordinates[i].x, &info.monster_coordinates[i].y,
         m3, m4, m5, m6);
        info.monster_types[i] = mt;
        monsters_fd[i]=i;
        
        
        pid=fork();
        
        if(pid==0)
        { //monster
            dup2(fdm[i][1], 0);
            dup2(fdm[i][1], 1);
            close(fdm[i][0]);
            close(fdm[i][1]);

            args[0] = monsterPath;
            args[1]=m3;
            args[2]=m4;
            args[3]=m5;
            args[4]=m6;
            args[5]=NULL;

            
            execv(monsterPath, args);
        }
        else{
            close(fdm[i][1]);
            monsters_pid[i]=pid;
            
        }
        


        


        
    }   
   
    sortCoordinates(monsters_pid, monsters_fd);
   
   
   
    

    // the game world should wait until it receives the ready response from all child processes
    player_response pr;
    monster_response mr;
    while(true){
        
        if(read(fdp[0], &pr, sizeof(pr))>0 && pr.pr_type==pr_ready){
            break;
        }
    }
    int count=0;
    
    for(int m=0;m<info.alive_monster_count;m++){
        while(true){
            if(read(fdm[m][0], &mr, sizeof(mr))>0 && mr.mr_type==mr_ready){
                break;
            }
        }
    }
    //print the initial state of the map
    print_map(&info);
    
    

    
   
    
    
   
    
    player_message pm;
    game_over_status go;
    
    //The first message from the game world to the player should contain the initial position of the player.
    pm.new_position.x=info.player.x, pm.new_position.y=info.player.y; 
    //The total damage inflicted on the player will be 0 for the first turn.
    pm.total_damage = 0;
    pm.game_over = false;
    
    // at the beginning damage on the monster is 0
    mm.damage=0;
    mm.game_over=false;
    int dead_count_per_turn, total_damage_from_prev_turn;
    dead_count_per_turn=0; total_damage_from_prev_turn=0;
    while(true){
       /*  printf("printing monsters coordinates array from info \n");
        for(int v=0;v<initial_monster_count;v++){
            printf("monster %c current position: (%d, %d)\n", info.monster_types[v], info.monster_coordinates[v].x, info.monster_coordinates[v].y);
        } */

        //printf("TURN -----> %d \n", turn_count);
        pm.alive_monster_count=info.alive_monster_count;
        pm.total_damage = total_damage_from_prev_turn;
        memcpy(&pm.monster_coordinates, &info.monster_coordinates, info.alive_monster_count*sizeof(coordinate));
        
        //printf("turn %d -> player message -> new_coordinate : (%d,%d), total_damage : %d\n", turn_count, pm.new_position.x,pm.new_position.y,pm.total_damage);
        
        write(fdp[0], &pm, sizeof(pm));  //send turn message to the player
        total_damage_from_prev_turn=0;
        
        // receive response from the player
        ssize_t eof;
        
        while((eof=read(fdp[0], &pr, sizeof(pr)))<=0){
            if(eof==0){
               
                sortCoordinates(monsters_pid, monsters_fd);
                go =go_left;
                gameOver(go);
                
                closeAll();
                return 0;

            }

        } 
        //printf("player response --> %d\n", pr.pr_type);
        
        
        
        

        
        if(pr.pr_type==pr_dead){
            // TODO the game world should send all entities game over signal without waiting for their order in the turn
            //printf("player dead game over\n");
            sortCoordinates(monsters_pid, monsters_fd);
            go = go_died;
            gameOver(go);
            closeAll();
            return 0;
        }
        else if(pr.pr_type==pr_move){
            //printf("player moving\n");
            if(isDoor(&pr.pr_content.move_to)){
                //printf("player won reached door \n");
                info.player.x=pr.pr_content.move_to.x;
                info.player.y=pr.pr_content.move_to.y;
                sortCoordinates(monsters_pid, monsters_fd);
                go=go_reached;
                gameOver(go);
                pm.game_over=true;
                write(fdp[0], &pm, sizeof(pm));
                closeAll();
                return 0;

            }
            else if(isWall(&pr.pr_content.move_to) || isOccupied(&pr.pr_content.move_to, -1)){
                //printf("cannot move\n");
            }

            else{
                
                pm.new_position.x=pr.pr_content.move_to.x;
                pm.new_position.y=pr.pr_content.move_to.y;
                info.player.x = pr.pr_content.move_to.x;
                info.player.y = pr.pr_content.move_to.y;
                //printf("player wants to move to (%d, %d) \n", pr.pr_content.move_to.x, pr.pr_content.move_to.y);
                //printf("player moved to (%d, %d) \n", info.player.x, info.player.y);

            }

            

        }

        int fd_i;
        dead_count_per_turn=0;
        for(int i=0;i<info.alive_monster_count;i++){
            
            fd_i = monsters_fd[i]; // to process info immediately read from monsters according to sorted coordinates
            
            mm.player_coordinate.x = info.player.x; mm.player_coordinate.y = info.player.y;
            
            // current monster_cord
            mm.new_position.x = info.monster_coordinates[i].x;
            mm.new_position.y = info.monster_coordinates[i].y;
            

            
            if(pr.pr_type==pr_attack){
                mm.damage = pr.pr_content.attacked[i];

            }
            else{ // pr_move
                mm.damage=0;

            }
            
            
            //printf(" %c monster message -> new position: (%d, %d), damage: %d, player coordinate: (%d,%d)\n", info.monster_types[i], mm.new_position.x, mm.new_position.y, mm.damage, mm.player_coordinate.x, mm.player_coordinate.y );
            write(fdm[fd_i][0], &mm, sizeof(mm));
            
            while(read(fdm[fd_i][0], &mr, sizeof(mr))<=0);
            //printf("%c  monster response ---> %d\n", info.monster_types[i], mr.mr_type);

            if(mr.mr_type==mr_dead){
                //printf("%c monster is dead\n", info.monster_types[i]);
                
                // TODO dead things
                
                //dead info with -1
                info.monster_coordinates[i].x=-1;
                info.monster_coordinates[i].y=-1;
                dead_count_per_turn++;
                waitpid(monsters_pid[i], &status, 0);
                close(fdm[fd_i][0]);
                
            }
            else if(mr.mr_type==mr_move){
                //printf(" %c monster is moving \n", info.monster_types[i]);
                if(((mr.mr_content.move_to.x==info.player.x && mr.mr_content.move_to.y==info.player.y)) || 
                isOccupied(&mr.mr_content.move_to,i) || isWall(&mr.mr_content.move_to) || isDoor(&mr.mr_content.move_to)){
                    //do nothing cannot move
                    //printf(" %c monster cannot move to (%d, %d) \n", info.monster_types[i], mr.mr_content.move_to.x, mr.mr_content.move_to.y);
                    /* if((mr.mr_content.move_to.x==info.player.x && mr.mr_content.move_to.y==info.player.y)){
                        printf("because player is here %d %d\n", info.player.x,info.player.y);
                    }
                    if(isOccupied(&mr.mr_content.move_to,i)){
                        printf("occupied by another monster\n");
                    }
                    if(isWall(&mr.mr_content.move_to)){
                        printf("it is wall\n");
                    }
                    if(isDoor(&mr.mr_content.move_to)){
                        printf("it is door\n");
                    } */
                    
                }
                else{
                    //printf(" %c monster wants to move to (%d, %d) \n", info.monster_types[i], mr.mr_content.move_to.x, mr.mr_content.move_to.y);
                    info.monster_coordinates[i].x=mr.mr_content.move_to.x;
                    info.monster_coordinates[i].y=mr.mr_content.move_to.y;
                    //printf(" %c monster moved to (%d, %d) \n", info.monster_types[i], info.monster_coordinates[i].x, info.monster_coordinates[i].y);
                }
            }
            else{ // attack
                //printf(" %c monster is attacking with  %d \n", info.monster_types[i], mr.mr_content.attack);
                total_damage_from_prev_turn += mr.mr_content.attack;
            }
            
            
            


        }
        info.alive_monster_count-=dead_count_per_turn;
        //printf("alive monster count: %d\n", info.alive_monster_count);
        if(info.alive_monster_count==0){
            //printf("all monsters killed player won \n");
            sortCoordinates(monsters_pid, monsters_fd);
            go=go_survived;
            gameOver(go);
            pm.game_over=true;
            write(fdp[0], &pm, sizeof(pm));
            closeAll();
            return 0;
        }
        
        sortCoordinates( monsters_pid, monsters_fd);
        print_map(&info);
        
        
        
    }
    
    

    return 0;
    
   
}
