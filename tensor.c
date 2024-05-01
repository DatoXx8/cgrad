#include <assert.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "tensor.h"

char name[BUFFER_NAME_SIZE + 1] = {'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', 'a', '\0'};
void name_update(char *name) {
    for(int64_t i = 0; i < BUFFER_NAME_SIZE - 1; i++) {
        assert(name[i] >= 'a' && name[i] <= 'z');
        if(name[i] != 'z') {
            name[i]++;
            return;
        } else {
            name[i] = 'a';
        }
    }
}
buffer_t buffer_alloc(int64_t a, int64_t z, int64_t y, int64_t x) {
    assert(a > 0);
    assert(z > 0);
    assert(y > 0);
    assert(x > 0);
    buffer_t buffer = {
        .a_inh = a,
        .z_inh = z,
        .y_inh = y,
        .x_inh = x,
        .a_sze = a,
        .z_sze = z,
        .y_sze = y,
        .x_sze = x,
        .a_str = z * y * x,
        .z_str = y * x,
        .y_str = x,
        .x_str = 1,
        .val = calloc(a * z * y * x, sizeof(double)),
        .sim_a_sze = a,
        .sim_z_sze = z,
        .sim_y_sze = y,
        .sim_x_sze = x,
        .sim_a_str = z * y * x,
        .sim_z_str = y * x,
        .sim_y_str = x,
        .sim_x_str = 1,
    };
    assert(buffer.val);
    strncpy(buffer.name, name, BUFFER_NAME_SIZE + 1);
    name_update(name);
    return buffer;
}
void buffer_free(buffer_t *buffer) {
    free(buffer->val);
}

/* Not a special or tested value at all. This is pure intuition. */
const int64_t INITIAL_CHILD_NUMBER = 8;
/* However there is a max of two parents per lazyop. */
const int64_t MAX_PARENT_NUMBER = 2;
op_t op_alloc(void) {
    op_t op = {
        .parent_capacity = MAX_PARENT_NUMBER,
        .parent = calloc(MAX_PARENT_NUMBER, sizeof(op_t *)),
        .child_capacity = INITIAL_CHILD_NUMBER,
        .child = calloc(INITIAL_CHILD_NUMBER, sizeof(op_t *)),
    };
    assert(op.parent);
    assert(op.child);
    return op;
}
void op_add_parents(op_t *op, op_t *output_parent, op_t *input_parent) {
    assert(op);
    if(output_parent) {
        if(output_parent->child_capacity == output_parent->child_count) {
            output_parent->child_capacity *= 2;
            output_parent->child = reallocarray(output_parent->child, output_parent->child_capacity, sizeof(op_t *));
            assert(output_parent->child);
        }
        output_parent->child[output_parent->child_count++] = op;

        op->parent[op->parent_count++] = output_parent;
    }
    if(input_parent) {
        if(input_parent->child_capacity == input_parent->child_count) {
            input_parent->child_capacity *= 2;
            input_parent->child = reallocarray(input_parent->child, input_parent->child_capacity, sizeof(op_t *));
            assert(input_parent->child);
        }
        input_parent->child[input_parent->child_count++] = op;

        op->parent[op->parent_count++] = input_parent;
    }
    assert(op->parent_count <= 2);
}
void op_free(op_t *op) {
    assert(op);
    assert(op->parent);
    free(op->parent);
    assert(op->child);
    free(op->child);
}
void op_cleanup(op_t *op) {
    assert(op);
    if(op->tensor_base) {
        tensor_t *tensor = (tensor_t *) op->tensor_base;
        tensor->op = NULL;
    }
    int64_t found;
    for(int64_t i = 0; i < op->child_count; i++) {
        found = 0;
        for(int64_t j = 0; j < op->child[i]->parent_count; j++) {
            if(op->child[i]->parent[j] == op) { found = 1; }
            if(found) {
                if(j == op->child[i]->parent_count - 1) {
                    op->child[i]->parent[j] = NULL;
                } else {
                    op->child[i]->parent[j] = op->child[i]->parent[j + 1];
                }
            }
        }
        op->child[i]->parent_count--;
        op->child[i] = NULL;
    }
    op->child_count = 0;
}
void op_single_print(op_t *op, int padding, int offset, const char *name) {
    if(strncmp(name, "", 1)) { printf("%*s%s\n", offset, "", name); }
    printf("%*s<%p> ", offset + padding, "", (void *) op);
    // switch(op->type) {
    //     case operation_unary: {
    //         switch(op->unary_type) {
    //             case unary_add: {
    //                 printf("U add {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                        op->out_buffer->x_sze, op->out_buffer->off, op->var_unary, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_subtract: {
    //                 printf("U sub {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                        op->out_buffer->x_sze, op->out_buffer->off, op->var_unary, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_multiply: {
    //                 printf("U mul {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                        op->out_buffer->x_sze, op->out_buffer->off, op->var_unary, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_divide: {
    //                 printf("U div {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                        op->out_buffer->x_sze, op->out_buffer->off, op->var_unary, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_exp: {
    //                 printf("U exp {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                 op->out_buffer->x_sze,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_log: {
    //                 printf("U log {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                 op->out_buffer->x_sze,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_square: {
    //                 printf("U sqr {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                 op->out_buffer->x_sze,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_sqrt: {
    //                 printf("U sqt {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                 op->out_buffer->x_sze,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_reciprocal: {
    //                 printf("U rcp {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                 op->out_buffer->x_sze,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_max: {
    //                 printf("U max {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                        op->out_buffer->x_sze, op->out_buffer->off, op->var_unary, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_min: {
    //                 printf("U min {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                        op->out_buffer->x_sze, op->out_buffer->off, op->var_unary, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_set: {
    //                 printf("U set {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                        op->out_buffer->x_sze, op->out_buffer->off, op->var_unary, (void *) op->out_buffer);
    //                 break;
    //             }
    //             /* Never *ever* use this for things like encryption, where the randomnes of the numbers is important! */
    //             case unary_random: {
    //                 printf("U ran {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                 op->out_buffer->x_sze,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_tanh: {
    //                 printf("U tnh {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                 op->out_buffer->x_sze,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_absolute: {
    //                 printf("U abs {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                 op->out_buffer->x_sze,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case unary_sign: {
    //                 printf("U sgn {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze, op->out_buffer->y_sze,
    //                 op->out_buffer->x_sze,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //         }
    //         break;
    //     }
    //     case operation_binary: {
    //         switch(op->binary_type) {
    //             case binary_add: {
    //                 printf("B add {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_subtract: {
    //                 printf("B add {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_multiply: {
    //                 printf("B mul {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_divide: {
    //                 printf("B div {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_max: {
    //                 printf("B max {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_min: {
    //                 printf("B min {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_copy: {
    //                 printf("B cpy {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_add_like: {
    //                 printf("L add {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_subtract_like: {
    //                 printf("L sub {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_multiply_like: {
    //                 printf("L mul {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_divide_like: {
    //                 printf("L div {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_max_like: {
    //                 printf("L max {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_min_like: {
    //                 printf("L min {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //             case binary_copy_like: {
    //                 printf("L cpy {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
    //                 break;
    //             }
    //         }
    //         break;
    //     }
    //     case operation_reduce: {
    //         switch(op->reduce_type) {
    //             case reduce_sum: {
    //                 printf("R sum {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) (void *) op->in_buffer);
    //                 break;
    //             }
    //             case reduce_avg: {
    //                 printf("R avg {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) (void *) op->in_buffer);
    //                 break;
    //             }
    //             case reduce_max: {
    //                 printf("R max {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) (void *) op->in_buffer);
    //                 break;
    //             }
    //             case reduce_min: {
    //                 printf("R min {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->in_buffer->a_sze, op->in_buffer->z_sze,
    //                        op->in_buffer->y_sze, op->in_buffer->x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) (void *) op->in_buffer);
    //                 break;
    //             }
    //         }
    //         break;
    //     }
    //     case operation_move: {
    //         switch(op->move_type) {
    //             case move_reshape: {
    //                 printf("M rsp {%lu, %lu, %lu, %lu} %lu > {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->var_a, op->var_z, op->var_y, op->var_x,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case move_resize: {
    //                 printf("M rsz {%lu, %lu, %lu, %lu} %lu > {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->var_a, op->var_z, op->var_y, op->var_x,
    //                        op->out_buffer->off, (void *) op->out_buffer);
    //                 break;
    //             }
    //             case move_offset: {
    //                 printf("M off {%lu, %lu, %lu, %lu} %lu > {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze, op->out_buffer->off, op->out_buffer->a_sze, op->out_buffer->z_sze,
    //                        op->out_buffer->y_sze, op->out_buffer->x_sze,
    //                        op->out_buffer->a_str * op->var_a + op->out_buffer->z_str * op->var_z + op->out_buffer->y_str * op->var_y +
    //                            op->out_buffer->x_str * op->var_x,
    //                        (void *) op->out_buffer);
    //                 break;
    //             }
    //         }
    //         break;
    //     }
    // }
    switch(op->type) {
        case operation_unary: {
            switch(op->unary_type) {
                case unary_add: {
                    printf("U add {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->var_unary, (void *) op->out_buffer);
                    break;
                }
                case unary_subtract: {
                    printf("U sub {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->var_unary, (void *) op->out_buffer);
                    break;
                }
                case unary_multiply: {
                    printf("U mul {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->var_unary, (void *) op->out_buffer);
                    break;
                }
                case unary_divide: {
                    printf("U div {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->var_unary, (void *) op->out_buffer);
                    break;
                }
                case unary_exp: {
                    printf("U exp {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case unary_log: {
                    printf("U log {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case unary_square: {
                    printf("U sqr {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case unary_sqrt: {
                    printf("U sqt {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case unary_reciprocal: {
                    printf("U rcp {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case unary_max: {
                    printf("U max {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->var_unary, (void *) op->out_buffer);
                    break;
                }
                case unary_min: {
                    printf("U min {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->var_unary, (void *) op->out_buffer);
                    break;
                }
                case unary_set: {
                    printf("U set {%lu, %lu, %lu, %lu} %lu %lf [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->var_unary, (void *) op->out_buffer);
                    break;
                }
                /* Never *ever* use this for things like encryption, where the randomnes of the numbers is important! */
                case unary_random: {
                    printf("U ran {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case unary_tanh: {
                    printf("U tnh {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case unary_absolute: {
                    printf("U abs {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case unary_sign: {
                    printf("U sgn {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze, op->out_buffer->sim_y_sze,
                           op->out_buffer->sim_x_sze, op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
            }
            break;
        }
        case operation_binary: {
            switch(op->binary_type) {
                case binary_add: {
                    printf("B add {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_subtract: {
                    printf("B add {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_multiply: {
                    printf("B mul {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_divide: {
                    printf("B div {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_max: {
                    printf("B max {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_min: {
                    printf("B min {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_copy: {
                    printf("B cpy {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_add_like: {
                    printf("L add {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_subtract_like: {
                    printf("L sub {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_multiply_like: {
                    printf("L mul {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_divide_like: {
                    printf("L div {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_max_like: {
                    printf("L max {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_min_like: {
                    printf("L min {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
                case binary_copy_like: {
                    printf("L cpy {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) op->in_buffer);
                    break;
                }
            }
            break;
        }
        case operation_reduce: {
            switch(op->reduce_type) {
                case reduce_sum: {
                    printf("R sum {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) (void *) op->in_buffer);
                    break;
                }
                case reduce_avg: {
                    printf("R avg {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) (void *) op->in_buffer);
                    break;
                }
                case reduce_max: {
                    printf("R max {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) (void *) op->in_buffer);
                    break;
                }
                case reduce_min: {
                    printf("R min {%lu, %lu, %lu, %lu} %lu < {%lu, %lu, %lu, %lu} %lu [%p] [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->in_buffer->sim_a_sze, op->in_buffer->sim_z_sze,
                           op->in_buffer->sim_y_sze, op->in_buffer->sim_x_sze, op->in_buffer->off, (void *) op->out_buffer, (void *) (void *) op->in_buffer);
                    break;
                }
            }
            break;
        }
        case operation_move: {
            switch(op->move_type) {
                case move_reshape: {
                    printf("M rsp {%lu, %lu, %lu, %lu} %lu > {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->var_a, op->var_z, op->var_y, op->var_x,
                           op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case move_resize: {
                    printf("M rsz {%lu, %lu, %lu, %lu} %lu > {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->var_a, op->var_z, op->var_y, op->var_x,
                           op->out_buffer->sim_off, (void *) op->out_buffer);
                    break;
                }
                case move_offset: {
                    printf("M off {%lu, %lu, %lu, %lu} %lu > {%lu, %lu, %lu, %lu} %lu [%p]\n", op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze, op->out_buffer->sim_off, op->out_buffer->sim_a_sze, op->out_buffer->sim_z_sze,
                           op->out_buffer->sim_y_sze, op->out_buffer->sim_x_sze,
                           op->out_buffer->sim_a_str * op->var_a + op->out_buffer->sim_z_str * op->var_z + op->out_buffer->sim_y_str * op->var_y +
                               op->out_buffer->sim_x_str * op->var_x,
                           (void *) op->out_buffer);
                    break;
                }
            }
            break;
        }
    }
}
void op_print(op_t *op, int padding, int offset, const char *name) {
    if(!op) { return; }
    if(strncmp(name, "", 1)) { printf("%*s%s\n", offset, "", name); }
    if(op == NULL) {
        printf("%*sNULL\n", offset + padding, "");
    } else {
        op_single_print(op, padding, offset, "");
        for(int64_t i = 0; i < op->parent_count; i++) { op_print(op->parent[i], padding, offset + padding, ""); }
    }
}
void op_single_op_cpu_realize(op_t *op) {
    switch(op->type) {
        case operation_unary: {
            switch(op->unary_type) {
                case unary_add: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) { BUFFER_AT_(op->out_buffer, a, z, y, x) += op->var_unary; }
                            }
                        }
                    }
                    break;
                }
                case unary_subtract: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) { BUFFER_AT_(op->out_buffer, a, z, y, x) -= op->var_unary; }
                            }
                        }
                    }
                    break;
                }
                case unary_multiply: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) { BUFFER_AT_(op->out_buffer, a, z, y, x) *= op->var_unary; }
                            }
                        }
                    }
                    break;
                }
                case unary_divide: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) { BUFFER_AT_(op->out_buffer, a, z, y, x) /= op->var_unary; }
                            }
                        }
                    }
                    break;
                }
                case unary_exp: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) = exp(BUFFER_AT_(op->out_buffer, a, z, y, x));
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_log: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) = log(BUFFER_AT_(op->out_buffer, a, z, y, x));
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_square: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) *= BUFFER_AT_(op->out_buffer, a, z, y, x);
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_sqrt: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) = sqrt(BUFFER_AT_(op->out_buffer, a, z, y, x));
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_reciprocal: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) = 1 / BUFFER_AT_(op->out_buffer, a, z, y, x);
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_max: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) =
                                        BUFFER_AT_(op->out_buffer, a, z, y, x) > op->var_unary ? BUFFER_AT_(op->out_buffer, a, z, y, x) : op->var_unary;
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_min: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) =
                                        BUFFER_AT_(op->out_buffer, a, z, y, x) < op->var_unary ? BUFFER_AT_(op->out_buffer, a, z, y, x) : op->var_unary;
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_set: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) { BUFFER_AT_(op->out_buffer, a, z, y, x) = op->var_unary; }
                            }
                        }
                    }
                    break;
                }
                /* Never *ever* use this for things like encryption, where the randomnes of the numbers is important! */
                case unary_random: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) = ((double) rand() / RAND_MAX) * 2 - 1;
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_tanh: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) = tanh(BUFFER_AT_(op->out_buffer, a, z, y, x));
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_absolute: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) = fabs(BUFFER_AT_(op->out_buffer, a, z, y, x));
                                }
                            }
                        }
                    }
                    break;
                }
                case unary_sign: {
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    if(BUFFER_AT_(op->out_buffer, a, z, y, x) < 0) {
                                        BUFFER_AT_(op->out_buffer, a, z, y, x) = -1;
                                    } else if(BUFFER_AT_(op->out_buffer, a, z, y, x) == 0) {
                                        BUFFER_AT_(op->out_buffer, a, z, y, x) = 0;
                                    } else {
                                        BUFFER_AT_(op->out_buffer, a, z, y, x) = 1;
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
            }
            break;
        }
        case operation_binary: {
            switch(op->binary_type) {
                case binary_add: {
                    assert(op->out_buffer->a_sze == op->in_buffer->a_sze);
                    assert(op->out_buffer->z_sze == op->in_buffer->z_sze);
                    assert(op->out_buffer->y_sze == op->in_buffer->y_sze);
                    assert(op->out_buffer->x_sze == op->in_buffer->x_sze);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) += BUFFER_AT_(op->in_buffer, a, z, y, x);
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_subtract: {
                    assert(op->out_buffer->a_sze == op->in_buffer->a_sze);
                    assert(op->out_buffer->z_sze == op->in_buffer->z_sze);
                    assert(op->out_buffer->y_sze == op->in_buffer->y_sze);
                    assert(op->out_buffer->x_sze == op->in_buffer->x_sze);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) -= BUFFER_AT_(op->in_buffer, a, z, y, x);
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_multiply: {
                    assert(op->out_buffer->a_sze == op->in_buffer->a_sze);
                    assert(op->out_buffer->z_sze == op->in_buffer->z_sze);
                    assert(op->out_buffer->y_sze == op->in_buffer->y_sze);
                    assert(op->out_buffer->x_sze == op->in_buffer->x_sze);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) *= BUFFER_AT_(op->in_buffer, a, z, y, x);
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_divide: {
                    assert(op->out_buffer->a_sze == op->in_buffer->a_sze);
                    assert(op->out_buffer->z_sze == op->in_buffer->z_sze);
                    assert(op->out_buffer->y_sze == op->in_buffer->y_sze);
                    assert(op->out_buffer->x_sze == op->in_buffer->x_sze);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) /= BUFFER_AT_(op->in_buffer, a, z, y, x);
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_max: {
                    assert(op->out_buffer->a_sze == op->in_buffer->a_sze);
                    assert(op->out_buffer->z_sze == op->in_buffer->z_sze);
                    assert(op->out_buffer->y_sze == op->in_buffer->y_sze);
                    assert(op->out_buffer->x_sze == op->in_buffer->x_sze);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    if(BUFFER_AT_(op->out_buffer, a, z, y, x) < BUFFER_AT_(op->in_buffer, a, z, y, x)) {
                                        BUFFER_AT_(op->out_buffer, a, z, y, x) = BUFFER_AT_(op->in_buffer, a, z, y, x);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_min: {
                    assert(op->out_buffer->a_sze == op->in_buffer->a_sze);
                    assert(op->out_buffer->z_sze == op->in_buffer->z_sze);
                    assert(op->out_buffer->y_sze == op->in_buffer->y_sze);
                    assert(op->out_buffer->x_sze == op->in_buffer->x_sze);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    if(BUFFER_AT_(op->out_buffer, a, z, y, x) > BUFFER_AT_(op->in_buffer, a, z, y, x)) {
                                        BUFFER_AT_(op->out_buffer, a, z, y, x) = BUFFER_AT_(op->in_buffer, a, z, y, x);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_copy: {
                    assert(op->out_buffer->a_sze == op->in_buffer->a_sze);
                    assert(op->out_buffer->z_sze == op->in_buffer->z_sze);
                    assert(op->out_buffer->y_sze == op->in_buffer->y_sze);
                    assert(op->out_buffer->x_sze == op->in_buffer->x_sze);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) = BUFFER_AT_(op->in_buffer, a, z, y, x);
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_add_like: {
                    assert(op->in_buffer->a_sze == 1);
                    assert(op->in_buffer->z_sze == 1);
                    assert(op->in_buffer->y_sze == 1);
                    assert(op->in_buffer->x_sze == 1);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) += BUFFER_AT_(op->in_buffer, 0, 0, 0, 0);
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_subtract_like: {
                    assert(op->in_buffer->a_sze == 1);
                    assert(op->in_buffer->z_sze == 1);
                    assert(op->in_buffer->y_sze == 1);
                    assert(op->in_buffer->x_sze == 1);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) -= BUFFER_AT_(op->in_buffer, 0, 0, 0, 0);
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_multiply_like: {
                    assert(op->in_buffer->a_sze == 1);
                    assert(op->in_buffer->z_sze == 1);
                    assert(op->in_buffer->y_sze == 1);
                    assert(op->in_buffer->x_sze == 1);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) *= BUFFER_AT_(op->in_buffer, 0, 0, 0, 0);
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_divide_like: {
                    assert(op->in_buffer->a_sze == 1);
                    assert(op->in_buffer->z_sze == 1);
                    assert(op->in_buffer->y_sze == 1);
                    assert(op->in_buffer->x_sze == 1);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) /= BUFFER_AT_(op->in_buffer, 0, 0, 0, 0);
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_max_like: {
                    assert(op->in_buffer->a_sze == 1);
                    assert(op->in_buffer->z_sze == 1);
                    assert(op->in_buffer->y_sze == 1);
                    assert(op->in_buffer->x_sze == 1);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    if(BUFFER_AT_(op->out_buffer, a, z, y, x) < BUFFER_AT_(op->in_buffer, 0, 0, 0, 0)) {
                                        BUFFER_AT_(op->out_buffer, a, z, y, x) = BUFFER_AT_(op->in_buffer, 0, 0, 0, 0);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_min_like: {
                    assert(op->in_buffer->a_sze == 1);
                    assert(op->in_buffer->z_sze == 1);
                    assert(op->in_buffer->y_sze == 1);
                    assert(op->in_buffer->x_sze == 1);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    if(BUFFER_AT_(op->out_buffer, a, z, y, x) > BUFFER_AT_(op->in_buffer, 0, 0, 0, 0)) {
                                        BUFFER_AT_(op->out_buffer, a, z, y, x) = BUFFER_AT_(op->in_buffer, 0, 0, 0, 0);
                                    }
                                }
                            }
                        }
                    }
                    break;
                }
                case binary_copy_like: {
                    assert(op->in_buffer->a_sze == 1);
                    assert(op->in_buffer->z_sze == 1);
                    assert(op->in_buffer->y_sze == 1);
                    assert(op->in_buffer->x_sze == 1);
                    for(int64_t a = 0; a < op->out_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->out_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->out_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->out_buffer->x_sze; x++) {
                                    BUFFER_AT_(op->out_buffer, a, z, y, x) = BUFFER_AT_(op->in_buffer, 0, 0, 0, 0);
                                }
                            }
                        }
                    }
                    break;
                }
            }
            break;
        }
        case operation_reduce: {
            switch(op->reduce_type) {
                case reduce_sum: {
                    assert(op->out_buffer->a_sze == 1);
                    assert(op->out_buffer->z_sze == 1);
                    assert(op->out_buffer->y_sze == 1);
                    assert(op->out_buffer->x_sze == 1);
                    double temp = 0;
                    for(int64_t a = 0; a < op->in_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->in_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->in_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->in_buffer->x_sze; x++) { temp += BUFFER_AT_(op->in_buffer, a, z, y, x); }
                            }
                        }
                    }
                    BUFFER_AT_(op->out_buffer, 0, 0, 0, 0) = temp;
                    break;
                }
                case reduce_max: {
                    assert(op->out_buffer->a_sze == 1);
                    assert(op->out_buffer->z_sze == 1);
                    assert(op->out_buffer->y_sze == 1);
                    assert(op->out_buffer->x_sze == 1);
                    double temp = -INFINITY;
                    for(int64_t a = 0; a < op->in_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->in_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->in_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->in_buffer->x_sze; x++) {
                                    if(temp < BUFFER_AT_(op->in_buffer, a, z, y, x)) { temp = BUFFER_AT_(op->in_buffer, a, z, y, x); }
                                }
                            }
                        }
                    }
                    BUFFER_AT_(op->out_buffer, 0, 0, 0, 0) = temp;
                    break;
                }
                case reduce_avg: {
                    assert(op->out_buffer->a_sze == 1);
                    assert(op->out_buffer->z_sze == 1);
                    assert(op->out_buffer->y_sze == 1);
                    assert(op->out_buffer->x_sze == 1);
                    double temp = 0;
                    for(int64_t a = 0; a < op->in_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->in_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->in_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->in_buffer->x_sze; x++) { temp += BUFFER_AT_(op->in_buffer, a, z, y, x); }
                            }
                        }
                    }
                    BUFFER_AT_(op->out_buffer, 0, 0, 0, 0) = temp / (op->in_buffer->a_sze * op->in_buffer->a_sze * op->in_buffer->a_sze * op->in_buffer->a_sze);
                    break;
                }
                case reduce_min: {
                    assert(op->out_buffer->a_sze == 1);
                    assert(op->out_buffer->z_sze == 1);
                    assert(op->out_buffer->y_sze == 1);
                    assert(op->out_buffer->x_sze == 1);
                    double temp = INFINITY;
                    for(int64_t a = 0; a < op->in_buffer->a_sze; a++) {
                        for(int64_t z = 0; z < op->in_buffer->z_sze; z++) {
                            for(int64_t y = 0; y < op->in_buffer->y_sze; y++) {
                                for(int64_t x = 0; x < op->in_buffer->x_sze; x++) {
                                    if(temp > BUFFER_AT_(op->in_buffer, a, z, y, x)) { temp = BUFFER_AT_(op->in_buffer, a, z, y, x); }
                                }
                            }
                        }
                    }
                    BUFFER_AT_(op->out_buffer, 0, 0, 0, 0) = temp;
                    break;
                }
            }
            break;
        }
        case operation_move: {
            switch(op->move_type) {
                case move_reshape: {
                    op->out_buffer->a_sze = op->var_a;
                    op->out_buffer->z_sze = op->var_z;
                    op->out_buffer->y_sze = op->var_y;
                    op->out_buffer->x_sze = op->var_x;
                    op->out_buffer->a_str = op->var_z * op->var_y * op->var_x;
                    op->out_buffer->z_str = op->var_y * op->var_x;
                    op->out_buffer->y_str = op->var_x;
                    op->out_buffer->x_str = 1;
                    break;
                }
                case move_resize: {
                    op->out_buffer->a_sze = op->var_a;
                    op->out_buffer->z_sze = op->var_z;
                    op->out_buffer->y_sze = op->var_y;
                    op->out_buffer->x_sze = op->var_x;
                    break;
                }
                case move_offset: {
                    op->out_buffer->off = op->out_buffer->a_str * op->var_a + op->out_buffer->z_str * op->var_z + op->out_buffer->y_str * op->var_y +
                                          op->out_buffer->x_str * op->var_x;
                    break;
                }
            }
            break;
        }
    }
}
void op_cpu_realize(op_t *op) {
    // assert(op);
    // while(op->parent_count > 0) { op_cpu_realize(op->parent[0]); }
    // op_single_op_cpu_realize(op);
    // op_cleanup(op);
    // op_free(op);
    // free(op);
    if(!op) { return; }
    op_t *temp;
    op_t *next = op;
    while(op->parent_count > 0) {
        temp = next;
        for(int64_t i = 0; i < MAX_DEPTH; i++) {
            if(temp->parent_count > 0) {
                temp = temp->parent[0];
            } else {
                break;
            }
        }
        assert(temp);
        assert(temp->parent_count == 0);
        op_single_op_cpu_realize(temp);
        next = temp->child_count > 0 ? temp->child[0] : op;
        op_cleanup(temp);
        op_free(temp);
        free(temp);
    }
    op_single_op_cpu_realize(temp);
    op_cleanup(op);
    op_free(op);
    free(op);
}

tensor_t tensor_alloc(int64_t a, int64_t z, int64_t y, int64_t x) {
    assert(a > 0);
    assert(z > 0);
    assert(y > 0);
    assert(x > 0);
    tensor_t tensor = {
        .op = NULL,
        .buffer = malloc(sizeof(buffer_t)),
    };
    assert(tensor.buffer);
    *tensor.buffer = buffer_alloc(a, z, y, x);
    return tensor;
}
void tensor_free(tensor_t *tensor) {
    if(tensor->op) {
        op_free(tensor->op);
        free(tensor->op);
    }
    buffer_free(tensor->buffer);
    free(tensor->buffer);
}

void tensor_add_unary(tensor_t *tensor, double value) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_add;
    tensor->op->var_unary = value;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_subtract_unary(tensor_t *tensor, double value) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_subtract;
    tensor->op->var_unary = value;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_multiply_unary(tensor_t *tensor, double value) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_multiply;
    tensor->op->var_unary = value;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_divide_unary(tensor_t *tensor, double value) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_divide;
    tensor->op->var_unary = value;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_exp_unary(tensor_t *tensor) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_exp;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_log_unary(tensor_t *tensor) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_log;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_square_unary(tensor_t *tensor) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_square;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_sqrt_unary(tensor_t *tensor) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_sqrt;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_reciprocal_unary(tensor_t *tensor) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_reciprocal;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_max_unary(tensor_t *tensor, double value) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_max;
    tensor->op->var_unary = value;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_min_unary(tensor_t *tensor, double value) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_min;
    tensor->op->var_unary = value;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_set_unary(tensor_t *tensor, double value) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_set;
    tensor->op->var_unary = value;
    tensor->op->out_buffer = tensor->buffer;
}
/* Never *ever* use this for things like encryption, where the randomnes of the numbers is important! */
void tensor_random_unary(tensor_t *tensor) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_random;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_tanh_unary(tensor_t *tensor) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_tanh;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_absolute_unary(tensor_t *tensor) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_absolute;
    tensor->op->out_buffer = tensor->buffer;
}
void tensor_sign_unary(tensor_t *tensor) {
    assert(tensor);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_unary;
    tensor->op->unary_type = unary_sign;
    tensor->op->out_buffer = tensor->buffer;
}

void tensor_add_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_add;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_subtract_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_subtract;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_multiply_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_multiply;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_divide_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_divide;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_max_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_max;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_min_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_min;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_copy_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_copy;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_add_like_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_add_like;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_subtract_like_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_subtract_like;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_multiply_like_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_multiply_like;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_divide_like_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_divide_like;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_max_like_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_max_like;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_min_like_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_min_like;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_copy_like_binary(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_binary;
    out->op->binary_type = binary_copy_like;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}

void tensor_sum_reduce(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_reduce;
    out->op->reduce_type = reduce_sum;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_max_reduce(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_reduce;
    out->op->reduce_type = reduce_max;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_avg_reduce(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_reduce;
    out->op->reduce_type = reduce_avg;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}
void tensor_min_reduce(tensor_t *out, tensor_t *in) {
    assert(out);
    assert(in);
    op_t *out_parent = out->op;
    out->op = malloc(sizeof(op_t));
    assert(out->op);
    *out->op = op_alloc();
    op_add_parents(out->op, out_parent, in->op);
    out->op->tensor_base = out;
    if(out_parent) { out_parent->tensor_base = NULL; }
    out->op->type = operation_reduce;
    out->op->reduce_type = reduce_min;
    out->op->out_buffer = out->buffer;
    out->op->in_buffer = in->buffer;
}

/* A per-dim size should never be `0`. */
void tensor_reshape_move(tensor_t *tensor, int64_t a, int64_t z, int64_t y, int64_t x) {
    assert(tensor);
    assert(a > 0);
    assert(z > 0);
    assert(y > 0);
    assert(x > 0);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_move;
    tensor->op->move_type = move_reshape;
    tensor->op->out_buffer = tensor->buffer;
    tensor->op->var_a = a;
    tensor->op->var_z = z;
    tensor->op->var_y = y;
    tensor->op->var_x = x;
}
/* A per-dim size should never be `0`. */
void tensor_resize_move(tensor_t *tensor, int64_t a, int64_t z, int64_t y, int64_t x) {
    assert(tensor);
    assert(a > 0);
    assert(z > 0);
    assert(y > 0);
    assert(x > 0);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    *tensor->op = op_alloc();
    assert(tensor->op);
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_move;
    tensor->op->move_type = move_resize;
    tensor->op->out_buffer = tensor->buffer;
    tensor->op->var_a = a;
    tensor->op->var_z = z;
    tensor->op->var_y = y;
    tensor->op->var_x = x;
}
/* A per-dim offset should never be `0`. */
void tensor_offset_move(tensor_t *tensor, int64_t a, int64_t z, int64_t y, int64_t x) {
    assert(tensor);
    assert(a >= 0);
    assert(z >= 0);
    assert(y >= 0);
    assert(x >= 0);
    op_t *parent = tensor->op;
    tensor->op = malloc(sizeof(op_t));
    assert(tensor->op);
    *tensor->op = op_alloc();
    op_add_parents(tensor->op, parent, NULL);
    tensor->op->tensor_base = tensor;
    if(parent) { parent->tensor_base = NULL; }
    tensor->op->type = operation_move;
    tensor->op->move_type = move_offset;
    tensor->op->out_buffer = tensor->buffer;
    tensor->op->var_a = a;
    tensor->op->var_z = z;
    tensor->op->var_y = y;
    tensor->op->var_x = x;
}

void tensor_cpu_realize(tensor_t *tensor) {
    assert(tensor);
    if(tensor->op) { op_cpu_realize(tensor->op); }
}

void tensor_print(tensor_t *tensor, int padding, int offset, const char *name) {
    assert(tensor);
    if(strncmp(name, "", 1)) {
        printf("%*s%s NAME: %s\n", offset, "", name, tensor->buffer->name);
    } else {
        printf("%*sNAME: %s\n", offset, "", tensor->buffer->name);
    }
    for(int64_t a = 0; a < tensor->buffer->a_sze; a++) {
        if(a) {
            printf("\n");
            printf("\n");
        }
        for(int64_t z = 0; z < tensor->buffer->z_sze; z++) {
            if(z) { printf("\n"); }
            for(int64_t y = 0; y < tensor->buffer->y_sze; y++) {
                printf("%*s[ ", offset + padding, "");
                for(int64_t x = 0; x < tensor->buffer->x_sze; x++) { printf("% lf ", BUFFER_AT_(tensor->buffer, a, z, y, x)); }
                printf("]\n");
            }
        }
    }
}
const int64_t A_MAX = 2;
const int64_t Z_MAX = 2;
const int64_t Y_MAX = 4;
const int64_t X_MAX = 4;
/* Just prints a `{2, 2, 4, 4}` subsection of the tensor. If name is `""` it doesn't print a new empty line where the name would have been. */
void tensor_preview(tensor_t *tensor, int padding, int offset, const char *name) {
    assert(tensor);
    if(strncmp(name, "", 1)) {
        printf("%*s%s sim_NAME: %s\n", offset, "", name, tensor->buffer->name);
    } else {
        printf("%*ssim_NAME: %s\n", offset, "", tensor->buffer->name);
    }
    for(int64_t a = 0; a < tensor->buffer->a_sze; a++) {
        if(a >= A_MAX) {
            printf("%*s...\n\n", offset, "");
            break;
        }
        if(a) { printf("\n\n"); }
        for(int64_t z = 0; z < tensor->buffer->z_sze; z++) {
            if(z >= Z_MAX) {
                printf("%*s...\n", offset, "");
                break;
            }
            if(z) { printf("\n"); }
            for(int64_t y = 0; y < tensor->buffer->y_sze; y++) {
                if(y >= Y_MAX) {
                    printf("%*s...\n", offset + padding, "");
                    break;
                }
                printf("%*s[ ", offset + padding, "");
                for(int64_t x = 0; x < tensor->buffer->x_sze; x++) {
                    if(x >= X_MAX) {
                        printf("...");
                        break;
                    }
                    printf("% lf ", BUFFER_AT_(tensor->buffer, a, z, y, x));
                }
                printf("]\n");
            }
        }
    }
}
