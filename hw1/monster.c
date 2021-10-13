#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/types.h>
#include<string.h>
#include<sys/wait.h>
#include<sys/socket.h>
#include "message.h"



int max(int a,int b){
    if(a>b){
        return a;
    }
    return b;
    
}

int main(int argc, char *argv[]) {
    
    char *p;
    int health = strtol(argv[1], &p, 10);
    int damage_to_player = strtol(argv[2], &p, 10);
    int defence = strtol(argv[3], &p, 10);
    int monster_range = strtol(argv[4], &p, 10);

    monster_response mr;
    mr.mr_type = mr_ready; //ready
    mr.mr_content.attack=0;
    write(1, &mr, sizeof(mr));
    monster_message mm;
    bool alive=true;
    while(alive){
            
        read(0, &mm, sizeof(mm));
        coordinate new_pos = mm.new_position;
        int damage_on_monster = mm.damage;
        coordinate player_coordinate = mm.player_coordinate;
        bool game_over = mm.game_over;
        if(game_over){
            return 0;
        }
        monster_response mr;
        //mr.mr_type=1;
        //mr.mr_content.move_to.x=3; mr.mr_content.move_to.y=4;


        //When the player is in the range of the monster, if the distance (no diagonal moves) between
            //two is less than or equal to the range, the monster will attack.

        int playerX = player_coordinate.x, playerY = player_coordinate.y;
        int monsterX = new_pos.x, monsterY = new_pos.y;
        int current_dist = abs(playerX-monsterX)+abs(playerY-monsterY);
        health = health-max(damage_on_monster-defence,0);
        bool dead = health<=0 ? true: false;
        if(dead){
            mr.mr_type = 3; //dead
            mr.mr_content.attack=0;
            alive=false;
        }
        else{
            if( current_dist <= monster_range){
                mr.mr_type = 2; // attack
                mr.mr_content.attack = damage_to_player;    
            }
            // If not, the monster will try to get closer to the player by moving one of the adjacent cells:
                // up,upper-right,right,bottom-right,down,bottom-left,left,upper-left.
            else{
                mr.mr_type = 1; //move
                coordinate move_to;
                move_to.x= monsterX, move_to.y= monsterY;
                int min_dist = current_dist;
                if(abs(playerX-(monsterX))+abs(playerY-(monsterY-1)) < min_dist){ //up
                    min_dist=abs(playerX-(monsterX))+abs(playerY-(monsterY-1));
                    move_to.x=monsterX;
                    move_to.y=monsterY-1;
                }
                if(abs(playerX-(monsterX+1))+abs(playerY-(monsterY-1)) < min_dist){ //upper-right
                    min_dist=abs(playerX-(monsterX+1))+abs(playerY-(monsterY-1));
                    move_to.x = monsterX+1;
                    move_to.y=monsterY-1;
                }
                if(abs(playerX-(monsterX+1))+abs(playerY-(monsterY)) < min_dist){ // right
                    min_dist=abs(playerX-(monsterX+1))+abs(playerY-(monsterY));
                    move_to.x = monsterX+1;
                    move_to.y=monsterY;
                }
                if(abs(playerX-(monsterX+1))+abs(playerY-(monsterY+1)) < min_dist){ // bottom-right
                    min_dist=abs(playerX-(monsterX+1))+abs(playerY-(monsterY+1));
                    move_to.x = monsterX+1;
                    move_to.y=monsterY+1;
                }
                if(abs(playerX-(monsterX))+abs(playerY-(monsterY+1)) < min_dist){ //bottom
                    min_dist=abs(playerX-(monsterX))+abs(playerY-(monsterY+1));
                    move_to.x = monsterX;
                    move_to.y=monsterY+1;
                }
                if(abs(playerX-(monsterX-1))+abs(playerY-(monsterY+1)) < min_dist){ //bottom-left
                    min_dist=abs(playerX-(monsterX-1))+abs(playerY-(monsterY+1));
                    move_to.x = monsterX-1;
                    move_to.y=monsterY+1;
                }
                if(abs(playerX-(monsterX-1))+abs(playerY-(monsterY)) < min_dist){ //left
                    min_dist=abs(playerX-(monsterX-1))+abs(playerY-(monsterY));
                    move_to.x = monsterX-1;
                    move_to.y=monsterY;
                }
                if(abs(playerX-(monsterX-1))+abs(playerY-(monsterY-1)) < min_dist){ //upper-left
                    min_dist=abs(playerX-(monsterX-1))+abs(playerY-(monsterY-1));
                    move_to.x = monsterX-1;
                    move_to.y=monsterY-1;
                }
                
                
                
                
                
                
                
                
                mr.mr_content.move_to=move_to;
                
                



            }
            
        }
        
        write(1, &mr, sizeof(mr));
            
    }
    

    return 0;


}
