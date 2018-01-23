#include <stdlib.h>
#include <time.h>
#include <vector>
#include <iostream>

template <typename T>
class AaTree {
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
    
    AaTree() {
        root = NIL;
        first = NIL;
        last = NIL;
        vacant = NIL;
        length = 0;
    }
    
    int skew(int index) {
        if (index == NIL)
            return NIL;
            
        int left = nodes[index].left;
        
        if (left != NIL && nodes[left].is_red) {
            // Rotate right, potential double reds will be resolved one level higher.
            //      Y?          X?
            //   Xr    Z?  =>      Yr
            //     g              g  Z?
            std::cerr << "Must skew at " << nodes[index].value << ".\n";
            // black or NIL grandchild changes parent, so can become left child. Or not.
            nodes[index].left = nodes[left].right;
            // nodes change position
            nodes[left].right = index;
            // promoted left child takes our color
            nodes[left].is_red = nodes[index].is_red;
            // we take its color (red)
            nodes[index].is_red = true;
            // promotion done
            index = left;
        }
        
        return index;
    }
    
    int split(int index) {
        if (index == NIL)
            return NIL;
            
        int right = nodes[index].right;
        
        if (right != NIL && nodes[right].is_red) {
            int rightright = nodes[right].right;
            
            if (rightright != NIL && nodes[rightright].is_red) {
                // Rotate left with some recoloring.
                //    Xb              Yr
                //       Yr    =>  Xb    Zb
                //      g  Zr        g
                std::cerr << "Must split at " << nodes[index].value << ".\n";;
                // black or NIL grandchild changes parent
                nodes[index].right = nodes[right].left;
                // nodes change position
                nodes[right].left = index;
                // since black grandparent is now on the left, must make a black on the right
                nodes[rightright].is_red = false;
                // promotion done
                index = right;
            }
        }
        
        return index;
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
                index = skew(index);
                index = split(index);
            }
            else if (value > nodes[index].value) {
                int right = add(nodes[index].right, value);
                nodes[index].right = right;
                index = split(index);
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
    
    void remove_redden(int index) {
        if (nodes[index].is_red) {
            nodes[nodes[index].left].is_red = true;
            nodes[nodes[index].right].is_red = true;
        }
        else
            nodes[index].is_red = true;
    }
    
    void remove_materialize(int index, bool &immaterial_black) {
        if (nodes[index].is_red && immaterial_black) {
            nodes[index].is_red = false;
            immaterial_black = false;
        }
    }
    
    int remove_fix(int index) {
        // It seems like we'd need less operations usually.
        // After reddening the left: skew(index) + skew(right) + split(index)
        // After reddening the right: split(index)
        // After reddening the right grandchildren: skew(right) + skew(rightright) + split(index) + split(right)
        // But for simplicity do these five in this order which should cover all the cases.
        
        std::cerr << "Remove fixing at " << nodes[index].value << ".\n";
        
        index = skew(index);
        
        if (nodes[index].right != NIL)
            nodes[index].right = skew(nodes[index].right);
            
        if (nodes[index].right != NIL && nodes[nodes[index].right].right != NIL)
            nodes[nodes[index].right].right = skew(nodes[nodes[index].right].right);
            
        index = split(index);
        
        if (nodes[index].right != NIL)
            nodes[index].right = split(nodes[index].right);
        
        return index;
    }
    
    int remove_left(int index, T value, bool &immaterial_black) {
        std::cerr << "Removing left of " << nodes[index].value << ".\n";
        nodes[index].left = remove(nodes[index].left, value, immaterial_black);
        
        if (immaterial_black) {
            std::cerr << "We have an immaterial black.\n";
            remove_redden(nodes[index].right);
            remove_materialize(index, immaterial_black);
            index = remove_fix(index);
            remove_materialize(index, immaterial_black);
        }
        
        return index;
    }

    int remove_right(int index, T value, bool &immaterial_black) {
        std::cerr << "Removing right of " << nodes[index].value << ".\n";
        nodes[index].right = remove(nodes[index].right, value, immaterial_black);

        if (immaterial_black) {
            remove_redden(nodes[index].left);
            remove_materialize(index, immaterial_black);
            index = remove_fix(index);
            remove_materialize(index, immaterial_black);
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
            if (nodes[index].left != NIL) {
                // If there's a left child, find the greatest smaller element as replacement
                int replacement = nodes[index].left;
                
                while (nodes[replacement].right != NIL)
                    replacement = nodes[replacement].right;
                    
                nodes[index].value = nodes[replacement].value;
                return remove_left(index, nodes[replacement].value, immaterial_black);
            }
            else if (nodes[index].right != NIL) {
                // If only a right child, that must be a single red node, good for replacement
                int replacement = nodes[index].right;
                nodes[index].value = nodes[replacement].value;
                return remove_right(index, nodes[replacement].value, immaterial_black);
            }
            else {
                // Leaf, just remove
                immaterial_black = !nodes[index].is_red;
                deallocate(index);
                return NIL;
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
    
    AaTree<int> aa;

    for (int i=0; i<100; i++) {
        aa.add(random() % 100);
        aa.print();
        aa.check_black_height();
        
        aa.add(random() % 100);
        aa.print();
        aa.check_black_height();
        
        aa.remove(random() % 100);
        aa.print();
        aa.check_black_height();
    }
    
    //aa.print();
    
    //aa.check_black_height();
}
