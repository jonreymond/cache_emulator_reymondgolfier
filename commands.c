#include "commands.h"


int program_init(program_t* program) {
    M_EXIT_IF_NULL(program, sizeof(program_t));

    program->nb_lines = 0;
    program->allocated = INIT_ALLOCATED;

    program->listing = calloc(INIT_ALLOCATED, sizeof(command_t));
    M_EXIT_IF_NULL(program->listing, INIT_ALLOCATED * sizeof(command_t));

    return ERR_NONE;
}

int program_add_command(program_t* program, const command_t* command){
    M_EXIT_IF_NULL(program, sizeof(program_t));
    M_EXIT_IF_NULL(program->listing, program->allocated * sizeof(command_t));
    M_EXIT_IF_NULL(command, sizeof(command_t));

    M_REQUIRE(command->data_size == sizeof(word_t) || command->data_size == 1, ERR_BAD_PARAMETER, "data_size must be 4 (word) or 1 (byte%c", ')');
    M_REQUIRE(!(command->type == INSTRUCTION && command->data_size != sizeof(word_t)), ERR_BAD_PARAMETER, "instructions must have size of 4 (word%c", ')');
    M_REQUIRE(!(command->order == WRITE && command->type == INSTRUCTION), ERR_BAD_PARAMETER, "cannot write an instructio%c", 'n');
    M_REQUIRE(command->vaddr.page_offset % 8 == 0 || command->vaddr.page_offset % 8*sizeof(word_t) == 0, ERR_BAD_PARAMETER, "vaddr not correct%c", ' ');

    while (program->nb_lines >= program->allocated) {
        program->allocated *= 2;
        M_EXIT_IF_NULL(program->listing = realloc(program->listing, program->allocated * sizeof(command_t)), program->allocated * sizeof(command_t));
    }

    program->listing[program->nb_lines] = *command;

    ++(program->nb_lines);

    return ERR_NONE;
}

int program_shrink(program_t* program){
    M_EXIT_IF_NULL(program, sizeof(program_t));
    M_EXIT_IF_NULL(program->listing, program->allocated * sizeof(command_t));

    if (!(program->nb_lines <= 10)) {
        program->allocated = program->nb_lines;
        M_EXIT_IF_NULL(program->listing = realloc(program->listing, program->allocated * sizeof(command_t)),
                                                  program->allocated * sizeof(command_t));
    }

    return ERR_NONE;
}

// Returns the char corresponding to the correct order
char order_to_char(command_word_t order) {
    switch(order) {
        case READ : return 'R';
        case WRITE : return 'W';
        default : M_EXIT(ERR_BAD_PARAMETER, "order is not READ or WRITE%c", ' ');
            break;
    }

    return ERR_NONE;
}

// Returns the char corresponding to the correct type
char type_to_char(mem_access_t type) {
    switch(type) {
        case DATA : return 'D';
        case INSTRUCTION : return 'I';
        default : M_EXIT(ERR_BAD_PARAMETER, "type is not DATA or INSTRUCTION%c", ' ');
            break;
    }

    return ERR_NONE;
}

// Returns the character corresponding to the size of the data (word = 4, byte = 1)
char size_to_char(size_t data_size) {
    switch(data_size) {
        case sizeof(word_t) : return 'W';
        case 1 : return 'B';
        default : M_EXIT(ERR_BAD_PARAMETER, "data_size is not 4 or 1%c", ' ');
            break;
    }

    return ERR_NONE;
}

int program_print(FILE* output, const program_t* program){

    M_REQUIRE_NON_NULL(output);
    M_REQUIRE_NON_NULL(program);

    for (int i = 0; i < program->nb_lines; i++) {
        command_t command = program->listing[i];

        fprintf(output, "%c ", order_to_char(command.order));
        fprintf(output, "%c", type_to_char(command.type));

        if (command.type == DATA) {
            fprintf(output, "%c", size_to_char(command.data_size));
        }

        fprintf(output, " ");

        if (command.order == WRITE) {
            if (command.data_size == sizeof(word_t)) {
                fprintf(output, "0x%08" PRIX32 " ", command.write_data);
            } else {
                fprintf(output, "0x%02" PRIX32 " ", command.write_data);
            }
        }

        fprintf(output, "@0x%016" PRIX64 "\n", virt_addr_t_to_uint64_t(&command.vaddr));
    }

    return ERR_NONE;
}

//checks if the command load Word or Byte and updates the command
int word_or_byte(FILE* entree, command_t* command);

//If the command is a READ
int read_procedure(FILE* entree, command_t* command);

//If the command is a WRITE
int write_procedure(FILE* entree, command_t* command);

//convert a char written in hex to an int
int hex_char_to_int(char c);

//skip the blank caracters, return the file with the head at the correct emplacement
//must skip at least one caracter
int next_char(FILE* entree);

//handles the address part
int address_procedure(FILE* entree, command_t* command);

int program_read(const char* filename, program_t* program){
    FILE* entree = fopen(filename, "r");

    M_REQUIRE_NON_NULL(entree);
    size_t error_code = ERR_NONE;
    error_code = program_init(program);
    if(error_code != ERR_NONE){
        fclose(entree);
        return error_code;
    }

    size_t nb_command = 0;
    while(!feof(entree) && nb_command < 100){
        char r_w = fgetc(entree);

        if (!feof(entree)) {
            if(r_w == 'R'){
                error_code = read_procedure(entree, &(program->listing[nb_command]));
            }
            else if(r_w == 'W'){
                error_code = write_procedure(entree, &(program->listing[nb_command]));
            } else {
                fclose(entree);
                return ERR_BAD_PARAMETER;
            }
            if(error_code != ERR_NONE){
                fclose(entree);
                return error_code;
            }
            nb_command++;
        }
    }
    program->nb_lines = nb_command;
    return ERR_NONE;
}

int word_or_byte(FILE* entree, command_t* command) {
    char w_b = fgetc(entree);
    if(!(w_b == 'W' || w_b == 'B')) return ERR_BAD_PARAMETER;

    if(w_b == 'B'){
        command->data_size = 1;
    } else {
        command->data_size = sizeof(word_t);
    }
    return ERR_NONE;
    }

int read_procedure(FILE* entree, command_t* command){
    command->order = READ;
    size_t error_code = ERR_NONE;
    error_code = next_char(entree);
    if(error_code != ERR_NONE) return error_code;

    char i_d = fgetc(entree);
    if(!(i_d == 'I' || i_d == 'D')) return ERR_BAD_PARAMETER; //not a valid type
    if(i_d == 'D'){
        command->type = DATA;
        error_code = word_or_byte(entree, command);
        if(error_code != ERR_NONE) return error_code;
    }
    else {
        command->type = INSTRUCTION;
        command->data_size = sizeof(word_t);
    }

    error_code = next_char(entree);
    if(error_code != ERR_NONE) return error_code;
    command->write_data = 0;

    return address_procedure(entree, command);
}

int write_procedure(FILE* entree, command_t* command){
    command->order = WRITE;
    size_t error_code = ERR_NONE;
    error_code = next_char(entree);
    if(error_code != ERR_NONE) return error_code;

    if(fgetc(entree) != 'D') return ERR_BAD_PARAMETER;  //not a valid type
    command->type = DATA;

    error_code = word_or_byte(entree, command);
    if(error_code != ERR_NONE) return error_code;

    error_code = next_char(entree);
    if(error_code != ERR_NONE) return error_code;
    if(fgetc(entree) != '0' || fgetc(entree) != 'x') return ERR_BAD_PARAMETER;      //not a valid address

    word_t wdata = 0;
    int max = 0;
    char num = '0';
    if(feof(entree))return ERR_BAD_PARAMETER;   //the doc finishes to soon
    num = fgetc(entree);

    while(!isspace(num) && !feof(entree)){
        if(feof(entree)) return ERR_BAD_PARAMETER;   //the doc finishes to soon
        wdata = (wdata << 4) | hex_char_to_int(num);
        max += 4;
        num = fgetc(entree);
    }
    if(!(max <= 32)) return ERR_BAD_PARAMETER;  //the write_data is too long
    ungetc(num, entree);

    command->write_data = wdata;

    error_code = next_char(entree);
    if(error_code != ERR_NONE) return error_code;
    error_code = address_procedure(entree, command);
    if(error_code != ERR_NONE) return error_code;

    return ERR_NONE;
}

int hex_char_to_int(char c){
    if ('0' <= c && c <= '9')
        return c - '0';
    if ('A' <= c && c <= 'F')
        return c - 'A' + 10;

    M_EXIT(ERR_BAD_PARAMETER, "not a hexa value%c", ' ');

    return ERR_NONE;
}

int next_char(FILE* entree){
    int skipped = -1;
    char c = 'z';
    do {
        if(feof(entree)) return ERR_BAD_PARAMETER;  //the doc finishes to soon
        c = fgetc(entree);
        skipped ++;
    } while(isspace(c));

    ungetc(c, entree);
    if(skipped == 0) return ERR_BAD_PARAMETER;  //need a blank in the instruction
    return ERR_NONE;
}

int address_procedure(FILE* entree, command_t* command){
    if(fgetc(entree) != '@' || fgetc(entree) != '0' || fgetc(entree) != 'x') return ERR_BAD_PARAMETER; //not a valid address

    uint64_t addr = 0;
    char c = 'z';
    for(int i = 0; i < 16; ++i) {
        c = fgetc(entree);
        addr = (addr << 4) | hex_char_to_int(c);
    }
    if(fgetc(entree) != '\n') return ERR_BAD_PARAMETER;         //must have a \n at the end of the line
    return init_virt_addr64(&(command->vaddr), addr);
}

int program_free(program_t* program) {
    M_REQUIRE_NON_NULL(program);
    free(program->listing);
    program->listing = NULL;
    program->nb_lines = 0;
    program->allocated = 0;

    return ERR_NONE;
}
