#include "list.h"


int is_empty_list(const list_t* this) {
    if (this != NULL) {
        if (this->front == NULL && this->back == NULL) {
            return 1;
        }
    }
    return 0;
}

void init_list(list_t* this) {
    if (this != NULL) {
        this->front = NULL;
        this->back = NULL;
    }
}

void clear_list(list_t* this) {
    if (this != NULL) {
        for_all_nodes(node, this) {
            pop_front(this);
        }
    }
}

// Creates new node with value
node_t* new_node(const list_content_t* value) {
    node_t* new = calloc(1, sizeof(node_t));
    new->value = *value;

    return new;
}

// Puts node at the back of the list
void place_node_at_back(list_t* this, node_t* node) {
    this->back->next = node;

    node->previous = this->back;
    node->next = NULL;

    this->back = node;
}

node_t* push_back(list_t* this, const list_content_t* value) {
    if (this != NULL && value != NULL) {
        node_t* new = new_node(value);
        if (new == NULL) {
            return NULL;
        }

        if (this->front == NULL) { // list is empty
            this->front = new;
            this->back = new;
        } else {
            place_node_at_back(this, new);
        }

        return new;
    }

    return NULL;
}

node_t* push_front(list_t* this, const list_content_t* value){
    if (this != NULL && value != NULL) {
        node_t* new = new_node(value);
        if (new == NULL) {
            return NULL;
        }

        if (this->back == NULL) { // list is empty
            this->front = new;
            this->back = new;
        } else {
            this->front->previous = new;

            new->next = this->front;
            new->previous = NULL;

            this->front = new;
        }

        return new;
    }

    return NULL;
}

void pop_back(list_t* this) {
    if (this != NULL && this->back != NULL) {
        if (this->back->previous == NULL) {
            free(this->back);
            init_list(this);
        } else {
            this->back = this->back->previous;
            free(this->back->next);
            this->back->next = NULL;
        }
    }
}

void pop_front(list_t* this) {
    if (this != NULL && this->front != NULL) {
        if (this->front->next == NULL) {
            free(this->front);
            init_list(this);
        } else {
            this->front = this->front->next;
            free(this->front->previous);
            this->front->previous = NULL;
        }
    }
}

void move_back(list_t* this, node_t* node) {
    if (this != NULL && node != NULL) {
        if (node->previous != NULL && node->next != NULL) {
            node->previous->next = node->next;
            node->next->previous = node->previous;
            place_node_at_back(this, node);
        } else if(node->previous == NULL && node->next != NULL) {
            this->front = node->next;
            this->front->previous = NULL;
            place_node_at_back(this, node);
        }
    }
}

int print_list(FILE* stream, const list_t* this) {
    int i = 0;

    if (this != NULL && stream != NULL) {
        fprintf(stream, "(");
        for_all_nodes(node, this) {
            print_node(stream, node->value);

            if (node->next == NULL) {
                fprintf(stream, ")");
            } else {
                fprintf(stream, ", ");
            }
            i++;
        }
    }

    return i;
}

int print_reverse_list(FILE* stream, const list_t* this) {
    int i = 0;

    if (this != NULL && stream != NULL) {
        fprintf(stream, "(");
        for_all_nodes_reverse(node, this) {
            print_node(stream, node->value);

            if (node->previous == NULL) {
                fprintf(stream, ")");
            } else {
                fprintf(stream, ", ");
            }
            i++;
        }
    }

    return i;
}
