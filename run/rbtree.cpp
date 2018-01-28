#include <stdlib.h>
#include <time.h>
#include <vector>
#include <iostream>

template <typename T>
class RbTree {
public:
    static const int NIL = -1;
    
    struct Node {
        T value;
        bool is_red;
        int left;
        int right;
        int next;
        int prev;
        
        void init(T v) {
            // Must be called for reused entries, too
            value = v;
            is_red = true;
            left = NIL;
            right = NIL;
            next = NIL;
            prev = NIL;
        }
    };
    
    std::vector<Node> nodes;
    int root;
    int first;
    int last;
    int vacant;
    int length;
    
    RbTree() {
        root = NIL;
        first = NIL;
        last = NIL;
        vacant = NIL;
        length = 0;
    }
    
    int left_fix(int index) {
        if (index == NIL)
            return index;
            
        int left = nodes[index].left;
        if (left == NIL || !nodes[left].is_red)
            return index;
            
        int leftleft = nodes[left].left;
        int leftright = nodes[left].right;
        
        if (leftleft == NIL || !nodes[leftleft].is_red) {
            if (leftright == NIL || !nodes[leftright].is_red)
                return index;
                
            // Red-swapping left rotate
            nodes[left].right = nodes[leftright].left;
            nodes[leftright].left = left;
            leftleft = left;
            left = leftright;
        }
        
        if (nodes[index].is_red)
            abort();
        
        // Red-promoting right rotate
        nodes[index].left = nodes[left].right;
        nodes[left].right = index;
        nodes[leftleft].is_red = false;
        
        return left;
    }
    

    int right_fix(int index) {
        if (index == NIL)
            return index;
            
        int right = nodes[index].right;
        if (right == NIL || !nodes[right].is_red)
            return index;
            
        int rightright = nodes[right].right;
        int rightleft = nodes[right].left;
        
        if (rightright == NIL || !nodes[rightright].is_red) {
            if (rightleft == NIL || !nodes[rightleft].is_red)
                return index;
                
            // Red-swapping right rotate
            nodes[right].left = nodes[rightleft].right;
            nodes[rightleft].right = right;
            rightright = right;
            right = rightleft;
        }
        
        if (nodes[index].is_red)
            abort();
        
        // Red-promoting left rotate
        nodes[index].right = nodes[right].left;
        nodes[right].left = index;
        nodes[rightright].is_red = false;
        
        return right;
    }
    
    int allocate() {
        int index;
        
        if (vacant != NIL) {
            index = vacant;
            vacant = nodes[index].next;
        }
        else {
            index = nodes.size();
            nodes.push_back(Node());
        }
        
        if (last != NIL) {
            nodes[last].next = index;
            nodes[index].prev = last;
        }
        else
            first = index;
            
        last = index;
        length += 1;
        
        return index;
    }
    
    void deallocate(int index) {
        int prev = nodes[index].prev;
        int next = nodes[index].next;
        
        if (prev != NIL)
            nodes[prev].next = next;
        else
            first = next;
            
        if (next != NIL)
            nodes[next].prev = prev;
        else
            last = prev;
            
        nodes[index].next = vacant;
        vacant = index;
        
        length -= 1;
    }
    
    int add(int index, T value) {
        if (index == NIL) {
            index = allocate();
            nodes[index].init(value);
        }
        else {
            if (value < nodes[index].value) {
                int left = add(nodes[index].left, value);
                nodes[index].left = left;
                index = left_fix(index);
            }
            else if (value > nodes[index].value) {
                int right = add(nodes[index].right, value);
                nodes[index].right = right;
                index = right_fix(index);
            }
            else {
                nodes[index].value = value;
            }
        }
        
        return index;
    }
    
    void add(T value) {
        std::cerr << "Adding " << value << ".\n";
        root = add(root, value);
        nodes[root].is_red = false;
    }
    
    bool has(int index, T value) {
        if (index == NIL)
            return false;
        else if (value < nodes[index].value)
            return has(nodes[index].left, value);
        else if (value > nodes[index].value)
            return has(nodes[index].right, value);
        else
            return true;
    }
    
    bool has(T value) {
        return has(root, value);
    }
    
    void redden(int index) {
        if (nodes[index].is_red) {
            nodes[nodes[index].left].is_red = true;
            nodes[nodes[index].right].is_red = true;
        }
        else
            nodes[index].is_red = true;
    }
    
    void posess(int index, bool &immaterial_black) {
        if (nodes[index].is_red && immaterial_black) {
            nodes[index].is_red = false;
            immaterial_black = false;
        }
    }
    
    int remove_left(int index, T value, bool &immaterial_black) {
        std::cerr << "Removing left of " << nodes[index].value << ".\n";
        nodes[index].left = remove(nodes[index].left, value, immaterial_black);
        
        if (immaterial_black) {
            redden(nodes[index].right);
            posess(index, immaterial_black);
            index = right_fix(index);
            posess(index, immaterial_black);
            nodes[index].left = right_fix(nodes[index].left);
        }
        
        return index;
    }

    int remove_right(int index, T value, bool &immaterial_black) {
        std::cerr << "Removing right of " << nodes[index].value << ".\n";
        nodes[index].right = remove(nodes[index].right, value, immaterial_black);

        if (immaterial_black) {
            redden(nodes[index].left);
            posess(index, immaterial_black);
            index = left_fix(index);
            posess(index, immaterial_black);
            nodes[index].right = left_fix(nodes[index].right);
        }
        
        return index;
    }
    
    int remove(int index, T value, bool &immaterial_black) {
        if (index == NIL)
            return index;
        else if (value < nodes[index].value) {
            return remove_left(index, value, immaterial_black);
        }
        else if (value > nodes[index].value) {
            return remove_right(index, value, immaterial_black);
        }
        else {
            if (nodes[index].left == NIL) {
                if (nodes[index].right == NIL) {
                    // Leaf, just remove
                    immaterial_black = !nodes[index].is_red;
                    deallocate(index);
                    return NIL;
                }
                else {
                    // Only a single red right child
                    nodes[index].value = nodes[nodes[index].right].value;
                    deallocate(nodes[index].right);
                    nodes[index].right = NIL;
                    return index;
                }
            }
            else {
                if (nodes[index].right == NIL) {
                    // Only a single red left child
                    nodes[index].value = nodes[nodes[index].left].value;
                    deallocate(nodes[index].left);
                    nodes[index].left = NIL;
                    return index;
                }
                else {
                    // Find the greatest smaller element as replacement
                    int replacement = nodes[index].left;
                
                    while (nodes[replacement].right != NIL)
                        replacement = nodes[replacement].right;
                    
                    nodes[index].value = nodes[replacement].value;
                    return remove_left(index, nodes[replacement].value, immaterial_black);
                }
            }
        }
    }
    
    void remove(T value) {
        std::cerr << "Removing " << value << ".\n";
        bool immaterial_black = false;
        root = remove(root, value, immaterial_black);
    }

    int check_black_height(int index) {
        if (index == NIL)
            return 0;
            
        int left = nodes[index].left;
        int right = nodes[index].right;
            
        int left_height = check_black_height(left);
        if (left_height < 0)
            return left_height;
            
        int right_height = check_black_height(right);
        if (right_height < 0)
            return right_height;
        
        if (left_height != right_height)
            return -1;
            
        if (nodes[index].is_red && ((left != NIL && nodes[left].is_red) || (right != NIL && nodes[right].is_red)))
            return -2;
            
        return left_height + (nodes[index].is_red ? 0 : 1);
    }
    
    void check_black_height() {
        int bh = check_black_height(root);
        
        if (bh == -1) {
            std::cerr << "HEIGHT INCONSISTENCY!\n";
            abort();
        }
        else if (bh == -2) {
            std::cerr << "RED INCONSISTENCY!\n";
            abort();
        }
        else
            std::cerr << "Black heigh is " << bh << ".\n";
    }
    
    void print(int index, int indent) {
        if (index != NIL)
            print(nodes[index].right, indent + 4);
        
        for (int i=0; i<indent; i++)
            std::cerr << " ";
            
        if (index == NIL) {
            std::cerr << "-\n";
        }
        else {
            std::cerr << (nodes[index].is_red ? "R" : "B") << " " << nodes[index].value << "\n";
        }
        
        if (index != NIL)
            print(nodes[index].left, indent + 4);
    }
    
    void print() {
        print(root, 0);
    }
};


int main() {
    srandom(time(NULL));
    
    RbTree<int> rb;

    for (int i=0; i<100; i++) {
        rb.add(random() % 100);
        rb.print();
        rb.check_black_height();
        
        rb.add(random() % 100);
        rb.print();
        rb.check_black_height();
        
        rb.remove(random() % 100);
        rb.print();
        rb.check_black_height();
    }
    
    //rb.print();
    
    //rb.check_black_height();
}
