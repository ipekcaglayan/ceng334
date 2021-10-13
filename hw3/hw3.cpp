#include <fcntl.h>
#include <iostream>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>
#include <bitset>
#include "ext2fs.h"
#include "math.h"

using namespace std;
int block_size, bg_number, inode_size, found_block;
unsigned char *img, *desc_table;
ext2_super_block* super;

ext2_inode *src_inode, *created_inode, *target_dir_inode;

uint32_t created_inode_bg_index;

long created_inode_number;
long target_dir_inode_number, src_inode_number;
string new_file_name;
uint32_t target_bg;
uint32_t target_inode_index;
string src_file_name;
ext2_block_group_descriptor* src_bg_desc;
string source_path, dest_path;


vector<string> src_list, dest_list; 
ext2_inode *root_inode;

string get_name(char * name_array, int name_length){
    string result="";
    for(int i=0;i<name_length;i++){
        result+=name_array[i];
    }
    return result;
}
void find_inode(uint32_t inode_number, ext2_inode* &inode){
    ext2_block_group_descriptor *bg_desc;
    uint32_t bg_index, inode_index;
    bg_index = (inode_number-1)/super->inodes_per_group;
    inode_index = (inode_number-1)% super->inodes_per_group;
    
    bg_desc = (ext2_block_group_descriptor *) (desc_table+bg_index*sizeof(ext2_block_group_descriptor));
    
    inode = (ext2_inode*)&img[bg_desc->inode_table*block_size+inode_index*inode_size];
}
void set_meta_data(){
    created_inode->mode=EXT2_S_IFREG;
    created_inode->uid=src_inode->uid;
    created_inode->size=src_inode->size;
    created_inode->access_time=src_inode->access_time;
    created_inode->creation_time=src_inode->creation_time;
    created_inode->modification_time=src_inode->modification_time;
    created_inode->deletion_time=0;
    created_inode->gid=src_inode->gid;
    created_inode->link_count=1;
    created_inode->block_count_512=src_inode->block_count_512;
    created_inode->flags=src_inode->flags;
    created_inode->reserved=src_inode->reserved;
    //cout<<"buraya geliyo mu?"<<endl;
    int index,block;
    for(int i=0;i<EXT2_NUM_DIRECT_BLOCKS;i++){
        created_inode->direct_blocks[i]=src_inode->direct_blocks[i];
        if(src_inode->direct_blocks[i]!=0){
            block=src_inode->direct_blocks[i];
            //cout<<block<<endl;
            index = block% super->blocks_per_group;
           /*  cout<<index<<endl;
            cout<<"src_bg_des yok mu yav "<<endl;
            cout<<src_bg_desc->block_refmap<<endl;
            cout<<"blocks per group"<<super->blocks_per_group<<endl; */
            if(block_size==1024){
                img[src_bg_desc->block_refmap*block_size+ 4*(index-1)]++;

            }
            else{
                img[src_bg_desc->block_refmap*block_size+ 4*index]++;
            }

        }
    }
    created_inode->single_indirect=src_inode->single_indirect;
    created_inode->double_indirect=src_inode->double_indirect;
    created_inode->triple_indirect=src_inode->triple_indirect;
    

    

}


void find_and_allocate_free_inode(){
    bool found = false;
    created_inode_bg_index=0;
    ext2_block_group_descriptor *bg_desc;
    while(1){
        bg_desc = (ext2_block_group_descriptor *) (desc_table+created_inode_bg_index*sizeof(ext2_block_group_descriptor));
        for(long i=0; i< super->inodes_per_group/8;i++ ){
                bitset<8> byte = img[bg_desc->inode_bitmap*block_size+i];
                for(int j=0;j<8;j++){
                    
                    if(byte[j]==0){
                        byte[j]=1;
                        super->free_inode_count--;
                        bg_desc->free_inode_count--;
                        created_inode_number = 8*i+j+1;
                        //cout<<"find func içinde: "<<created_inode_number<< endl;
                        
                        img[bg_desc->inode_bitmap*block_size+i] += pow(2,j);
                        found=true;
                        //cout<<"found inode number: "<<created_inode_number<<endl;
                        break;
                    }
                }
                if(found){
                    break;
                }
        }
        if(found){
            break;
        }
        
        created_inode_bg_index++;
    }
    find_inode(created_inode_number, created_inode);
    //cout<<"created_inode link count"<<created_inode->link_count<<endl;
    set_meta_data();
}

void find_free_block(){
    //cout<<"find_free_block fonks çağrıldı"<<endl;
    bool found = false;
    int bg_index_fb=0;
    bool go_next_bg=false;
    ext2_block_group_descriptor *bg_desc;
    while(1){
        bg_desc = (ext2_block_group_descriptor *) (desc_table+bg_index_fb*sizeof(ext2_block_group_descriptor));
        uint32_t data_block_number = super->blocks_per_group - super->first_data_block; 
        int i=0;
        while(true){
            bitset<8> byte = img[bg_desc->block_bitmap*block_size+i];
            for(int j=0;j<8;j++){
                //cout<<"bit: "<<byte[j]<<endl;
                if(8*i+j>=super->blocks_per_group){
                    go_next_bg=true;
                    break;
                }
                if(byte[j]==0){
                    byte[j]=1;
                    
                    //cout<<"i: "<<i<< "j: "<<j<<endl;
                    found_block = 8*i+j;
                    
                    img[bg_desc->block_bitmap*block_size+i] += pow(2,j);
                    found=true;
                    break;
                }
            }
            
            if(go_next_bg){
                break;
            }
            
            if(found){
                break;
            }
            i++;
        }
        if(found){
            break;
        }
        
        bg_index_fb++;
    }
}






void allocate_new_direct_data(int direct_data_index){
    
    
}
void allocate_dir_entry(){
    bool entry_allocated=false;
    int block_size_counter=0;
    int new_data_block_index;
    int ofset;
    ext2_dir_entry * e, *new_entry;
    uint16_t last_entry_length;
    for(int i=0; i< EXT2_NUM_DIRECT_BLOCKS;i++){
        if(target_dir_inode->direct_blocks[i]>0){
            block_size_counter = 0;
            ofset = target_dir_inode->direct_blocks[i]*block_size;
            while(block_size_counter<block_size){
                e = (ext2_dir_entry *) &img[ofset];
                //there exists empty space in the data block so check if space is enough
                if(e->inode==0){
                    if( e->length>= new_file_name.length()+8){
                        e->inode = created_inode_number;
                        e->name_length=new_file_name.length();
                        strcpy(e->name, new_file_name.c_str());
                        entry_allocated=true;
                    }
                }
                if(entry_allocated){
                    return;
                }
                block_size_counter += e->length;
                ofset += e->length;
                
            }
            // bos entry yokmus o zaman sonuncuyu bölüp yeni olusturabilcez mi bakalım
        
        /*     cout<<"last entry space:  "<<e->length<<endl;
            cout<<"block size counter: "<<block_size_counter<<endl; */
            last_entry_length = e->name_length+8;
            //cout<<"last entry length "<<last_entry_length<<endl;
            if(last_entry_length%4){
                last_entry_length += 4-(last_entry_length%4); 
            }
            //cout<<"last entry length "<<last_entry_length<<endl;
         
            if(e->length-last_entry_length>=new_file_name.length()+8){
                ofset -=e->length;
                block_size_counter -= e->length; 
                e->length = last_entry_length;
                ofset+=e->length;
                block_size_counter +=e->length;
               // cout<<"block size counter: "<<block_size_counter<<endl;
                new_entry= (ext2_dir_entry *) &img[ofset];
                new_entry->inode = created_inode_number;
                new_entry->length = block_size-block_size_counter;
                new_entry->name_length=new_file_name.length();
                new_entry->file_type = 1;
                strcpy(new_entry->name, new_file_name.c_str());
                entry_allocated=true;
                return;

            }

        }
        else{
            new_data_block_index=i; // if allocating new block is required it will be inserted here
            break;
        }
        
    }
    if(entry_allocated){
        return;
    }
    //cout<<"buraya hiç gelmemesi lazimmmm"<<endl;
    find_free_block();
    target_dir_inode->direct_blocks[new_data_block_index] = found_block;
    ofset = found_block*block_size;
    e= (ext2_dir_entry *) &img[ofset];
    e->inode = created_inode_number;
    e->name_length=new_file_name.length();
    e->length = block_size;
    e->file_type=1;
    strcpy(e->name, new_file_name.c_str());
    ext2_block_group_descriptor* bg_desc_new_block;
    bg_desc_new_block = (ext2_block_group_descriptor *) (desc_table+(found_block/super->blocks_per_group)*sizeof(ext2_block_group_descriptor));
    if(block_size==1024){
        img[bg_desc_new_block->block_refmap*block_size+ 4*(found_block%super->blocks_per_group-1)]++;

    }
    else{
        img[bg_desc_new_block->block_refmap*block_size+ 4*(found_block%super->blocks_per_group)]++;

    }

    super->free_block_count--;
    bg_desc_new_block->free_block_count--;
    target_dir_inode->size +=block_size;
    target_dir_inode->block_count_512 += block_size/512;


}

void string_split(int is_source, string s){
    string del="/";
    int start = 0;
    int end = s.find(del);
    if( is_source){
        while (end != -1) {
            src_list.push_back(s.substr(start, end - start));
            start = end + del.size();
            end = s.find(del, start);
        }
        src_list.push_back(s.substr(start, end - start));

    }
    else{
        while (end != -1) {
            dest_list.push_back(s.substr(start, end - start));
            start = end + del.size();
            end = s.find(del, start);
        }
        new_file_name = s.substr(start, end - start);

    }
    

}



void find_asked_inode(int is_src,vector<string> list){
    ext2_inode * current_inode=root_inode;
    ext2_inode* next_inode;
    string name;
    ext2_block_group_descriptor *bg_desc_current;
    uint32_t bg_index_current, inode_index_current;
    int block_size_counter=0;
    int ofset;
    bool found =false;
    ext2_dir_entry * e;
    bool is_file=false;
    for(int i=1;i<list.size();i++){
        name = list[i];
        found=false;
        if(is_src && i==list.size()-1 ){
            is_file=true;
        }
        
        for(int j=0; j<EXT2_NUM_DIRECT_BLOCKS;j++){
            if(current_inode->direct_blocks[j]>0){
                //cout<<"direct block: "<<current_inode->direct_blocks[j] <<endl;
                block_size_counter = 0;
                ofset = current_inode->direct_blocks[j]*block_size;
                while(block_size_counter<block_size){
                    e= (ext2_dir_entry *) &img[ofset];
                    //cout<<e->name<<endl;
                    if(get_name(e->name, e->name_length)==name){
                        //cout<<"ne bu: "<< name<<endl;
                        if((e->file_type==2 && !is_file) || e->file_type==1 && is_file ){
                            found=true;
                            src_inode_number = e->inode;
                            bg_index_current = (src_inode_number-1)/super->inodes_per_group;
                            inode_index_current = (src_inode_number-1)% super->inodes_per_group;
                            
                            bg_desc_current = (ext2_block_group_descriptor *) (desc_table+bg_index_current*sizeof(ext2_block_group_descriptor));
                            
                            current_inode = (ext2_inode*)&img[bg_desc_current->inode_table*block_size+inode_index_current*inode_size];
                            break;      
                        }
                        
                    }
                    ofset+=e->length;
                    block_size_counter+=e->length;

                }
                if(found){
                    break;
                }
            }
        
        }   

    }
    if(is_src){
        src_inode=current_inode;
    }
    else{
        target_dir_inode=current_inode;
        target_dir_inode_number=src_inode_number;
    }
}

void dup_operation(){
    if(source_path[0]=='/'){ //full path
            
            string_split(1,source_path);
            find_asked_inode(1,src_list);
            //cout<<"source inode----> "<<src_inode_number<<endl;
            uint32_t src_bg_index = (src_inode_number-1)/super->inodes_per_group;
            src_bg_desc = (ext2_block_group_descriptor *) (desc_table+src_bg_index*sizeof(ext2_block_group_descriptor));
            //cout<<"alooo"<<endl;
            
        }
    else{ //inode
            long inode_number = stol(source_path);
            find_inode(inode_number, src_inode);
            uint32_t src_bg_index = (src_inode_number-1)/super->inodes_per_group;
            src_bg_desc = (ext2_block_group_descriptor *) (desc_table+src_bg_index*sizeof(ext2_block_group_descriptor));
            /* cout<<inode_number<<endl;
            cout<<"src inode link count"<<src_inode->link_count<<endl;
            for(int i=0; i< 12;i++){
                cout<<src_inode->direct_blocks[i]<<endl;
            } */
            
        }
        
        
        
    if(dest_path[0]=='/'){
            //full path
            string_split(0, dest_path);
            find_asked_inode(0, dest_list);
            //cout<<"dest inode----> "<<target_dir_inode_number<<endl;
            find_and_allocate_free_inode();
            //cout<<"free inode number "<<created_inode_number<<endl;
            allocate_dir_entry();      
        }
    else{
        int delimiter = dest_path.find('/');
        target_dir_inode_number = stol(dest_path.substr(0,delimiter));
        new_file_name = dest_path.substr(delimiter+1);
        find_inode(target_dir_inode_number, target_dir_inode);
        find_and_allocate_free_inode();
        allocate_dir_entry();
        /* cout<<"target inode number"<< target_dir_inode_number<<endl;
            cout<<"target inode link count"<<target_dir_inode->link_count<<endl;
            for(int i=0; i< 12;i++){
                cout<<target_dir_inode->direct_blocks[i]<<endl;
        } */
        

        

    }

}

int main(int argc, char** argv){
    found_block = -1;
    int fd = open(argv[2], O_RDWR, S_IRUSR| S_IWUSR);
    source_path = argv[3];
    dest_path = argv[4];
    struct stat file;
    fstat(fd, &file);

    img = (unsigned char*)mmap(NULL, file.st_size, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    super = (ext2_super_block*)(img+EXT2_BOOT_BLOCK_SIZE);
    //cout<<"main basinda free block sayisi: "<<super->free_block_count<<endl;
    block_size = EXT2_UNLOG(super->log_block_size);
    bg_number = super->block_count/super->blocks_per_group;
    inode_size = super->inode_size;
    desc_table = img+EXT2_BOOT_BLOCK_SIZE+EXT2_SUPER_BLOCK_SIZE;
    if(block_size==4096){
        desc_table = img+4096;
    }
    string operation = argv[1]; 
    
    find_inode(EXT2_ROOT_INODE, root_inode); // root inode found
    
    if(operation=="dup"){
        dup_operation();
        cout<<created_inode_number<<endl;
        cout<<found_block<<endl;
        //cout<<"en sonda free block sayisi: "<<super->free_block_count<<endl;

    }
    return 0;


}