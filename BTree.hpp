
#include <iostream>
#include <fstream>
#include <functional>
#include <cstddef>
#include <cstring>
#include <cstdio>
#include "exception.hpp"
#include "utility.hpp"

namespace sjtu {

    template <class Key, class Value, class Compare = std::less<Key> >
    class BTree {
    public:
        typedef std::pair<Key, Value> value_type;
        typedef long long int off_n;
#define buff(location) reinterpret_cast<char *>(&location)


    public:
        static const int UNIT=1024;
        static const size_t M =(UNIT-sizeof(int)-sizeof(bool))/(sizeof(Key)+sizeof(off_n))/2;
        static const size_t L =(UNIT-sizeof(int)-2*sizeof(off_n))/sizeof(value_type)/2;
        static const off_n core_pos=0;
        static const off_n root_pos=1;
        static const off_n leaf_head_pos=2;
        struct tree_node {
            int n;
            bool to_leaf;
            Key key[2*M];
            off_n c[2*M+1];

            tree_node() : n(0), to_leaf(false) {}
        };

        struct leaf_node {
            int n;
            off_n next;
            off_n pre;
            value_type data[2*L+1];

            leaf_node(off_n nextt=0,off_n pree=0):n(0),next(nextt),pre(pree) {}
        };

        char path[256];
        std::fstream io;

        struct Core
        {
            off_n end;
            long long int size;
            Core(off_n endd=3,long long int sizee=0):end(endd),size(sizee){}
        }core;

        void add_new_blocks(char *s,size_t _size=UNIT)
        {
            _write(s,core.end++);
        }
        inline void _write(char *s,off_n number_of_blocks,size_t _size=UNIT)
        {
            io.seekp(UNIT*number_of_blocks);
            io.write(s,_size);
            io.flush();
        }
        inline void _read(char *s,off_n number_of_blocks,size_t _size=UNIT)
        {
            io.seekg(UNIT*number_of_blocks);
            io.read(s,_size);
            io.flush();
        }
        //for copy build
        void file_copy(const char *src,const char *dst)
        {
            std::ifstream in(src,std::ios::binary);
            std::ofstream out(dst,std::ios::binary);
            if(!in)
            {
                std::cout<<"error in flie_copy open "<<src<<std::endl;
                exit(EXIT_FAILURE);
            }
            if(!out)
            {
                std::cout<<"error in flie_copy open "<<dst<<std::endl;
                exit(EXIT_FAILURE);
            }
            if(strcmp(src,dst)==0)
            {
                std::cout<<"the src file can't be the same as dst file"<<std::endl;
                exit(EXIT_FAILURE);
            }

            char buf[2048];
            while(in)
            {
                in.read(buf,2048);
                out.write(buf,in.gcount());
            }
            in.close();
            out.close();

        }
        //for insert
        void split_tree_node(tree_node &father,off_n pos,int idx)
        {
            off_n initial_pos=io.tellp();

            tree_node son,tem;
            _read(buff(son),father.c[idx]);

            //copy son M~2M-1 to tem
            //son.key[M-1]copy to father.key[idx]
            son.n=tem.n=M;
            tem.to_leaf=son.to_leaf;
            for(int i=0;i<M;i++)
                tem.c[i]=son.c[i+M];
            for(int i=0;i<M-1;i++)
                tem.key[i]=son.key[i+M];
            //modify father
            for(int i=father.n-1;i>idx;i--)
                father.c[i+1]=father.c[i];
            for(int i=father.n-2;i>=idx;i--)
                father.key[i+1]=father.key[i];
            father.key[idx]=son.key[M-1];
            father.n++;

            //write new_tree node to file
            father.c[idx+1]=core.end;
            add_new_blocks(buff(tem));
            //modify father to file
            _write(buff(father),pos);
            //modify son to file
            _write(buff(son),father.c[idx]);
            io.seekp(initial_pos);
        }
        inline void add_leaf_node(leaf_node &son,off_n pos,leaf_node &newnode)
        {
            //in charge of changing pointer of each list and modify it to the file
            off_n initial_pos=io.tellp();
            newnode.next=son.next;
            newnode.pre=pos;

            off_n newnode_pos=core.end;

            //modify: son->next->pre=newnode
            if(son.next)
            {
                leaf_node rson;
                _read(buff(rson),son.next);
                rson.pre=newnode_pos;
                _write(buff(rson),son.next);
            }
            //modify:son->next=newnode
            son.next=newnode_pos;
            _write(buff(son),pos);
            add_new_blocks(buff(newnode));

            io.seekp(initial_pos);
        }
        void split_leaf_node(tree_node &father,off_n pos,int idx)
        {
            off_n initial_pos=io.tellp();
            leaf_node son,tem;
            //read leaf son
            _read(buff(son),father.c[idx]);

            //copy son L~2L-1 to tem
            tem.n=son.n=L;
            for(int i=0;i<L;i++)
                tem.data[i]=son.data[i+L];
            //modify leaf to file
            add_leaf_node(son,father.c[idx],tem);
            //modify father
            for(int i=father.n-1;i>idx;i--)
                father.c[i+1]=father.c[i];
            for(int i=father.n-2;i>=idx;i--)
                father.key[i+1]=father.key[i];
            father.n++;
            father.key[idx]=tem.data[0].first;
            father.c[idx+1]=core.end-1;
            //write father to file
            _write(buff(father),pos);
            io.seekp(initial_pos);
        }
        void split_root(tree_node &old_root)
        {
            tree_node tem;
            tree_node new_root;
            //copy M~2M-1 to tem
            old_root.n=tem.n=M;
            for(int i=0;i<M;i++)
                tem.c[i]=old_root.c[i+M];
            for(int i=0;i<M-1;i++)
                tem.key[i]=old_root.key[i+M];
            tem.to_leaf=old_root.to_leaf;
            //modify to file and modify new_root
            new_root.n=2;
            new_root.key[0]=old_root.key[M-1];
            new_root.c[0]=core.end;
            add_new_blocks(buff(old_root));
            new_root.c[1]=new_root.c[0]+1;
            add_new_blocks(buff(tem));
            _write(buff(new_root),root_pos);
            io.flush();

        };
        int binary_search_treenode(tree_node &father,const Key&key)const
        {
            int l=0,r=father.n-2,mid=(l+r)>>1;
            if(key<father.key[l])
                return 0;
            if(father.key[father.n-2]<=key)
                return r+1;

            while(l<r)
            {
                if(key<father.key[mid])
                    r=mid;
                if(father.key[mid]<key)
                    l=mid+1;
                if(father.key[mid]==key)
                    return mid+1;
                mid=(l+r)>>1;
            }
            return mid;
        }
        //for insert,return the idx to insert
        int binary_search_leafnode(const leaf_node &lnode,const Key &key)
        {
            if(lnode.n==0)
                return 0;
            int l=0,r=lnode.n-1,mid=(l+r)>>1;
            if(lnode.data[0].first>key)
                return 0;
            if(lnode.data[lnode.n-1].first<key)
                return lnode.n;

            while(l<r)
            {
                if(lnode.data[mid].first>key)
                    r=mid;
                if(lnode.data[mid].first<key)
                    l=mid+1;
                if(lnode.data[mid].first==key)
                    return mid+1;
                mid=(l+r)>>1;
            }
            return mid;
        };
        int insert_on_leaf(leaf_node &lnode,const Key &key,const Value &value)
        {
            int idx=binary_search_leafnode(lnode,key);
            for(int i=lnode.n-1;i>=idx;i--)
                lnode.data[i+1]=lnode.data[i];
            lnode.data[idx].first=key;
            lnode.data[idx].second=value;
            lnode.n++;
            core.size++;
            return idx;
        }



        //ERASE FOUNCTIONS
        //
        //
        //
        //for earse,return the idx to insert
        //return -1 if the value does not exist
         int _binary_search_leafnode(const leaf_node &lnode,const Key &key)
         {
             int l=0,r=lnode.n-1,mid=(l+r)>>1;
             if(lnode.data[0].first>key)
                 return -1;
             if(lnode.data[lnode.n-1].second<key)
                 return -1;

             while(l<r)
             {
                 if(lnode.data[mid].first>key)
                     r=mid-1;
                 if(lnode.data[mid].first<key)
                     l=mid+1;
                 if(lnode.data[mid].first==key)
                     return mid;
                 mid=(l+r)>>1;
             }
             if(lnode.data[mid].first==key)
                 return mid;
             return -1;
         };
         void delete_root1(tree_node &root,tree_node &son1,tree_node &son2)
         {
             for(int i=0;i<M;i++)
                 son1.c[i+M]=son2.c[i];
             son1.key[M-1]=root.key[0];
             for(int i=0;i<M-1;i++)
                 son1.key[i+M]=son2.key[i];
             son1.n=2*M;
             _write(buff(son1),root_pos);
             //son2 is ignored,which may lead to waste.
         }
         inline void erase_in_leaf(leaf_node &lnode,const int &idx)
         {
             for(int i=idx;i<lnode.n-1;i++)
                 lnode.data[i]=lnode.data[i+1];
             lnode.n--;
             core.size--;
         }
         void tree_borrow_from_right(tree_node &father,off_n pos,int idx,tree_node &son,tree_node &rson)
         {
             son.key[M-1]=father.key[idx];
             father.key[idx]=rson.key[0];
             son.c[M]=rson.c[0];
             for(int i=0;i<rson.n-1;i++)
                 rson.c[i]=rson.c[i+1];
             for(int i=0;i<rson.n-2;i++)
                 rson.key[i]=rson.key[i+1];
             rson.n--;
             son.n++;

             _write(buff(father),pos);
             _write(buff(son),father.c[idx]);
             _write(buff(rson),father.c[idx+1]);
         }
         void tree_borrow_from_left(tree_node &father,off_n pos,int idx,tree_node &son,tree_node &lson)
         {
             for(int i=son.n;i>0;i--)
                 son.c[i]=son.c[i-1];
             for(int i=son.n-1;i>0;i--)
                 son.key[i]=son.key[i-1];
             son.c[0]=lson.c[lson.n-1];
             son.key[0]=father.key[idx-1];
             father.key[idx-1]=lson.key[lson.n-2];
             lson.n--;
             son.n++;
             _write(buff(father),pos);
             _write(buff(son),father.c[idx]);
             _write(buff(lson),father.c[idx-1]);

         }
         void leaf_borrow_from_right(tree_node &father,off_n pos,int idx,leaf_node &son,leaf_node &rson)
         {
             son.data[son.n++] = rson.data[0];
             for (int i=0;i<rson.n-1;i++)
                 rson.data[i]=rson.data[i+1];
             rson.n--;
             father.key[idx] = rson.data[0].first;
             _write(buff(father),pos);
             _write(buff(son),father.c[idx]);
             _write(buff(rson),father.c[idx+1]);
         }
         void leaf_borrow_from_left(tree_node &father,off_n pos,int idx,leaf_node &son,leaf_node &lson)
         {
             for(int i=son.n;i>0;i--)
                 son.data[i]=son.data[i-1];
             son.data[0]=lson.data[--lson.n];
             son.n++;
             father.key[idx-1]=son.data[0].first;
             _write(buff(father), pos);
             _write(buff(son),father.c[idx]);
             _write(buff(lson),father.c[idx-1]);
         }
         void tree_merge(tree_node &father,off_n pos,int idx,tree_node &lson,tree_node &rson)
         {
             //copy rson to lson
             lson.key[M]=father.key[idx];
             for(int i=0;i<M;i++)
                 lson.c[i+M]=rson.c[i];
             for(int i=0;i<M-1;i++)
                 lson.key[i+M]=rson.key[i];
             lson.n=2*M;
             rson.n=0;
             //modify father
             for(int i=idx;i<father.n-2;i++)
                 father.key[i]=father.key[i+1];
             for(int i=idx+1;i<father.n-1;i++)
                 father.c[i]=father.c[i+1];
             father.n--;
             //write to file
             _write(buff(father),pos);
             _write(buff(lson),father.c[idx]);
             //
         }
         void leaf_merge(tree_node &father,off_n pos,int idx,leaf_node &lson,leaf_node &rson)
         {
             //similar to tree_merge
             //copy rson to lson
             for(int i=0;i<L;i++)
                 lson.data[i+L]=rson.data[i];
             lson.next=rson.next;
             lson.n=2*L;
             //modify father
             for(int i=idx;i<father.n-2;i++)
                 father.key[i]=father.key[i+1];
             for(int i=idx+1;i<father.n-1;i++)
                 father.c[i]=father.c[i+1];
             father.n--;
             //write to file
             _write(buff(father),pos);
             _write(buff(lson),father.c[idx]);
         }

    public:

        class const_iterator
        {
        private:
        public:
            const_iterator(){};
        };
        class iterator {
        private:

            // Your private members go here
        public:
            bool modify(const Key& key){

            }
            iterator() {
                // TODO Default Constructor
            }
            iterator(const iterator& other) {
                // TODO Copy Constructor
            }
            // Return a new iterator which points to the n-next elements
            iterator operator++(int) {
                // Todo iterator++
            }
            iterator& operator++() {
                // Todo ++iterator
            }
            iterator operator--(int) {
                // Todo iterator--
            }
            iterator& operator--() {
                // Todo --iterator
            }
            // Overloaded of operator '==' and '!='
            // Check whether the iterators are same
            value_type& operator*() const {
                // Todo operator*, return the <K,V> of iterator
            }
            bool operator==(const iterator& rhs) const {
                // Todo operator ==
            }
            bool operator==(const const_iterator& rhs) const {
                // Todo operator ==
            }
            bool operator!=(const iterator& rhs) const {
                // Todo operator !=
            }
            bool operator!=(const const_iterator& rhs) const {
                // Todo operator !=
            }
            value_type* operator->() const noexcept {
                /**
                 * for the support of it->first.
                 * See
                 * <http://kelvinh.github.io/blog/2013/11/20/overloading-of-member-access-operator-dash-greater-than-symbol-in-cpp/>
                 * for help.
                 */
            }
        };


        BTree(const char *WritePath="BPlusTre.txt")
        {
            strcpy(path,WritePath);
            io.open(path,std::ios::in|std::ios::out|std::ios::binary);
            if(!io)
            {
                io.open(path,std::ios::out);
                io.close();
                io.open(path,std::ios::in|std::ios::out|std::ios::binary);
                leaf_node tem_leaf;
                tree_node tem_tree;

                _write(buff(tem_tree),root_pos);
                _write(buff(tem_leaf),leaf_head_pos);

            }
            else
            {
                _read(buff(core),core_pos);
            }
        }

//        BTree(const BTree& other)
//        {
//            // Todo Copy
//            path="copied_tree.txt";
//            //create a new file
//            std::fstream out(path,std::ios::out);
//            out.close();
//            file_copy(path,other.path);
//        }
//        BTree& operator=(const BTree& other)
//        {
//            // Todo Assignment
//            file_copy(path,other.path);
//        }
        ~BTree() {
            _write(buff(core),core_pos);
            io.close();
        }


        // Insert: Insert certain Key-Value into the database
        // Return a pair, the first of the pair is the iterator point to the new
        // element, the second of the pair is Success if it is successfully inserted

        std::pair<iterator, OperationResult> insert(const Key& key, const Value& value)
        {
            tree_node root;
            leaf_node leaf_head;
            _read(buff(root),root_pos);
            _read(buff(leaf_head),leaf_head_pos);
            //if there's only one leaf_node and leaf_node need not to split
            int insert_idx;
            if(!root.n&&leaf_head.n<2*L)
            {
                insert_idx=insert_on_leaf(leaf_head,key,value);
                _write(buff(leaf_head),leaf_head_pos);
                return std::pair<iterator,OperationResult>(iterator(),Success);
            }

            //if there's only one leaf_node and leaf_node needs to split
            // build root
            if(!root.n&&leaf_head.n==2*L)
            {

                leaf_node new_leaf;
                new_leaf.n=leaf_head.n=L;
                for(int i=0;i<L;i++)
                    new_leaf.data[i]=leaf_head.data[i+L];
                leaf_head.next=core.end;
                new_leaf.pre=leaf_head_pos;

                root.n=2;
                root.c[0]=leaf_head_pos;
                root.c[1]=leaf_head.next;
                root.key[0]=new_leaf.data[0].first;
                root.to_leaf=true;
                _write(buff(root),root_pos);
                _write(buff(leaf_head),leaf_head_pos);
                add_new_blocks(buff(new_leaf));

                return insert(key,value);
            }
            //else
            tree_node cur=root;
            off_n cur_pos=root_pos;
            //if root need to spilt
            if(cur.n==2*M)
            {
                split_root(cur);
                _read(buff(cur),root_pos);
            }
            int next_child;
            int next_size;
            while(!cur.to_leaf)
            {
                next_child=binary_search_treenode(cur,key);
                io.seekg(cur.c[next_child]*UNIT);
                io.read(buff(next_size),sizeof(int));
                if(next_size==2*M)
                {
                    split_tree_node(cur,cur_pos,next_child);
                    next_child=binary_search_treenode(cur,key);
                }
                cur_pos=cur.c[next_child];
                _read(buff(cur),cur.c[next_child]);
            }
            //cur.child is leaf
            next_child=binary_search_treenode(cur,key);
            io.seekg(cur.c[next_child]*UNIT);
            io.read(buff(next_size),sizeof(int));
            if(next_size==2*L)
            {
                split_leaf_node(cur,cur_pos,next_child);//it should not let io point to another place
                next_child=binary_search_treenode(cur,key);
            }
            leaf_node leaf_to_insert;
            cur_pos=cur.c[next_child];
            _read(buff(leaf_to_insert),cur.c[next_child]);

            insert_on_leaf(leaf_to_insert,key,value);

            _write(buff(leaf_to_insert),cur_pos);
            return std::pair<iterator,OperationResult>(iterator(),Success);
        }
        // Erase: Erase the Key-Value
        // Return Success if it is successfully erased
        // Return Fail if the key doesn't exist in the database
         OperationResult erase(const Key& key)
         {
             tree_node cur;
             //cur=root
             _read(buff(cur),root_pos);

             int idx;
             leaf_node lnode;
             if(!cur.n)
             {
                 _read(buff(lnode),leaf_head_pos);
                 idx=_binary_search_leafnode(lnode,key);
                 if(idx==-1)
                     return Fail;
                 erase_in_leaf(lnode,idx);
                 _write(buff(lnode),leaf_head_pos);
                 return Success;
             }
             // to process the possibility of root need to delete
             if(cur.n==2)
             {
                 if(cur.to_leaf)
                 {
                     leaf_node l1,l2;
                     _read(buff(l1),cur.c[0]);
                     _read(buff(l2),cur.c[1]);
                     if(l1.n==L&&l2.n==L)
                     {
                         int idxx = _binary_search_leafnode(l1, key);
                         if (idxx == -1)
                         {
                             idx = _binary_search_leafnode(l2, key);
                             if (idx == -1)
                                 return Fail;
                         }
                         //delete root
                         cur.n = 0;
                         for (int i = 0; i < L; i++)
                             l1.data[i+L] = l2.data[i];
                         l1.n = 2 * L;
                         l1.next=0;
                         idx = _binary_search_leafnode(l1, key);
                         erase_in_leaf(l1, idx);

                         _write(buff(cur), root_pos);
                         _write(buff(l1), leaf_head_pos);
                         return Success;
                     }
                 }
                 else//!cur.to_leaf
                 {
                     tree_node t1,t2;
                     _read(buff(t1),cur.c[0]);
                     _read(buff(t1),cur.c[1]);
                     if(t1.n==t2.n==M)
                     {
                         delete_root1(cur,t1,t2);
                         _read(buff(cur),root_pos);
                     }
                 }
             }
             off_n cur_pos=root_pos;
             tree_node son1,son2;

             while(!cur.to_leaf)
             {
                 idx=binary_search_treenode(cur,key);
                 _read(buff(son1),cur.c[idx]);
                 if(son1.n==M)
                 {
                     if(idx<cur.n-1)
                     {
                         _read(buff(son2),cur.c[idx+1]);
                         if(son2.n>M)
                             tree_borrow_from_right(cur,cur_pos,idx,son1,son2);
                         else
                             tree_merge(cur,cur_pos,idx,son1,son2);
                     }
                     else
                     {
                         _read(buff(son2),cur.c[idx-1]);
                         if(son2.n>M)
                             tree_borrow_from_left(cur,cur_pos,idx,son1,son2);
                         else
                             tree_merge(cur,cur_pos,idx-1,son2,son1);
                     }
                     idx=binary_search_treenode(cur,key);
                 }

                 cur_pos=cur.c[idx];
                 cur=son1;
             }

             //cur->to_leaf
             idx=binary_search_treenode(cur,key);
             _read(buff(lnode),cur.c[idx]);

             leaf_node lnode2;
             //if too less data
             if(lnode.n==L)
             {
                 if(idx<cur.n-1)
                 {
                     _read(buff(lnode2),cur.c[idx+1]);
                     if(lnode2.n>L)
                         leaf_borrow_from_right(cur,cur_pos,idx,lnode,lnode2);
                     else
                         leaf_merge(cur,cur_pos,idx,lnode,lnode2);
                 }
                 else
                 {
                     _read(buff(lnode2),cur.c[idx-1]);
                     if(lnode2.n>L)
                         leaf_borrow_from_left(cur,cur_pos,idx,lnode,lnode2);
                     else
                         leaf_merge(cur,cur_pos,idx-1,lnode2,lnode);
                 }
                 idx=binary_search_treenode(cur,key);
             }
             //erase data
             off_n leaf_pos=cur.c[idx];
             idx=_binary_search_leafnode(lnode,key);
             if(idx==-1)
                 return Fail;  // If you can't finish erase part, just remaining here.
             erase_in_leaf(lnode,idx);
             _write(buff(lnode),leaf_pos);
             return Success;
         }
         //Return a iterator to the beginning
         iterator begin()
         {
             return iterator();
         }
         const_iterator cbegin() const
         {
             return const_iterator();
         }
         // Return a iterator to the end(the next element after the last)
         iterator end()
         {
             return iterator();
         }
         const_iterator cend() const
         {
             return const_iterator();
         }
        // Check whether this BTree is empty
        bool empty()
        {
            return core.size==0;
        }
        // Return the number of <K,V> pairs

        size_t size()
        {
            return size_t(core.size);
        }
        // Clear the BTree
        void clear()
        {
            tree_node root;
            leaf_node leaf_head;
            _write(buff(root),root_pos);
            _write(buff(leaf_head),leaf_head_pos);
            core.size=0;
            core.end=3;
            _write(buff(core),core_pos);
        }
        /**
         * Returns the number of elements with key
         *   that compares equivalent to the specified argument,
         * The default method of check the equivalence is !(a < b || b > a)
         */
        size_t count(const Key& key)
        {
             tree_node cur;
             leaf_node lnode;
             _read(buff(cur),root_pos);
             int idx;
             if(!cur.n)
             {
                 _read(buff(lnode),leaf_head_pos);
                 idx=_binary_search_leafnode(lnode,key);
                 if(idx==-1)
                     return 0;
                 return 1;
             }
             while(!cur.to_leaf)
             {
                 idx=binary_search_treenode(cur,key);
                 _read(buff(cur),cur.c[idx]);
             }
             idx=binary_search_treenode(cur,key);
             _read(buff(lnode),cur.c[idx]);

             idx=_binary_search_leafnode(lnode,key);
             if(idx==-1)
                 return 0;
      }
        /**
         * Finds an element with key equivalent to key.
         * key value of the element to search for.
         * Iterator to an element with key equivalent to key.
         *   If no such element is found, past-the-end (see end()) iterator is
         * returned.
         */
         iterator find(const Key& key)
         {
//             tree_node cur;
//             leaf_node lnode;
//             _read(buff(cur),root_pos);
//             int idx;
//             if(!cur.n)
//             {
//                 _read(buff(lnode),leaf_head_pos);
//                 idx=_binary_search_leafnode(lnode,key);
//                 if(idx==-1)
//                     return end();
//                 return iterator();
//             }
//             while(!cur.to_leaf)
//             {
//                 idx=binary_search_treenode(cur,key);
//                 _read(buff(cur),cur.c[idx]);
//             }
//             idx=binary_search_treenode(cur,key);
//             _read(buff(lnode),cur.c[idx]);
//
//             idx=_binary_search_leafnode(lnode,key);
//             if(idx==-1)
//                 return end();
             return iterator();

         }
         const_iterator find(const Key& key) const
         {
             return const_iterator();
         }
         Value at(const Key& key)
         {
             tree_node cur;
             leaf_node lnode;
             off_n idx;
             _read(buff(cur),root_pos);

             if(!cur.n)
             {
                 _read(buff(lnode),leaf_head_pos);
                 idx=_binary_search_leafnode(lnode,key);
                 if(idx==-1)
                     return Value();
                 return lnode.data[idx].second;
             }
             while(!cur.to_leaf)
             {
                 idx=binary_search_treenode(cur,key);
                 _read(buff(cur),cur.c[idx]);
             }
             idx=binary_search_treenode(cur,cur.c[idx]);
             _read(buff(lnode),cur.c[idx]);
             idx=_binary_search_leafnode(lnode,key);
             if(idx==-1)
                 return Value();
             return lnode.data[idx].second;

         }
//         void traverse()
//         {
//             std::cout<<'\n'<<"in function traverse():"<<'\n';
//             leaf_node cur;
//             int count=0;
//             _read(buff(cur),leaf_head_pos);
//             if(!cur.n)
//             {
//                 std::cout<<"the tree is empty."<<'\n';
//                 return;
//             }
//             while(true)
//             {
//                 std::cout<<"the "<<count++<<"th leaf_node:"<<'\n';
//                 for(int i=0;i<cur.n;i++)
//                     std::cout<<cur.data[i].first<<" ";
//                 std::cout<<'\n';
//                 if(cur.next)
//                     _read(buff(cur),cur.next);
//                 else
//                     return;
//             }
//         }
    };
}  // namespace sjtu
