#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../tensor.h"
#include "CL/cl.h"
#include "codegen.h"
#include "compile.h"

/* TODO: Implement branchless
 * https://www.geeksforgeeks.org/compute-the-minimum-or-maximum-max-of-two-integers-without-branching/ */

static inline uint64_t max_u64(const uint64_t a, const uint64_t x) {
    if(a > x) {
        return a;
    } else {
        return x;
    }
}

static inline uint64_t min_u64(const uint64_t a, const uint64_t x) {
    if(a < x) {
        return a;
    } else {
        return x;
    }
}

static inline bool op_equal(const op_t *op1, const op_t *op2) {
    assert(op1);
    assert(op2);
    /* I don't think memcmp works here because I think the offsets being irrelevant would mess that up */
    /* Strictly speaking I could modify the struct layout so that I could cast the pointer to uint8_t[something] and
     * just compare the stuff we want to compare but that is straight up horrible and wayyyy to bug prone */
    return op1->type_op == op2->type_op && op1->type_unary == op2->type_unary && op1->type_binary == op2->type_binary &&
           op1->type_reduce == op2->type_reduce && op1->u_var == op2->u_var &&
           op1->buffer_out.name_off == op2->buffer_out.name_off && op1->buffer_out.a_sze == op2->buffer_out.a_sze &&
           op1->buffer_out.z_sze == op2->buffer_out.z_sze && op1->buffer_out.y_sze == op2->buffer_out.y_sze &&
           op1->buffer_out.x_sze == op2->buffer_out.x_sze && op1->buffer_in.name_off == op2->buffer_in.name_off &&
           op1->buffer_in.a_sze == op2->buffer_in.a_sze && op1->buffer_in.z_sze == op2->buffer_in.z_sze &&
           op1->buffer_in.y_sze == op2->buffer_in.y_sze && op1->buffer_in.x_sze == op2->buffer_in.x_sze;
}

/* This only really makes sense if the ops are equal by op_equal */
static inline bool op_overlaps(const op_t *op1, const op_t *op2) {
    assert(op1);
    assert(op2);
    /* TODO: Implement this for different sizes */
    assert(op1->buffer_out.a_sze == op2->buffer_out.a_sze);
    assert(op1->buffer_out.z_sze == op2->buffer_out.z_sze);
    assert(op1->buffer_out.y_sze == op2->buffer_out.y_sze);
    assert(op1->buffer_out.x_sze == op2->buffer_out.x_sze);

    const uint64_t a_1 = op1->buffer_out.a_off + 1;
    const uint64_t z_1 = op1->buffer_out.z_off + 1;
    const uint64_t y_1 = op1->buffer_out.y_off + 1;
    const uint64_t x_1 = op1->buffer_out.x_off + 1;

    const uint64_t a_2 = op2->buffer_out.a_off + 1;
    const uint64_t z_2 = op2->buffer_out.z_off + 1;
    const uint64_t y_2 = op2->buffer_out.y_off + 1;
    const uint64_t x_2 = op2->buffer_out.x_off + 1;

    /* For right now both buffers have to have the same size so it doesn't matter which we choose here */
    const uint64_t a_sze = op2->buffer_out.a_sze;
    const uint64_t z_sze = op2->buffer_out.z_sze;
    const uint64_t y_sze = op2->buffer_out.y_sze;
    const uint64_t x_sze = op2->buffer_out.x_sze;

    // if(op1->buffer_out.off == op2->buffer_out.off) {
    //     return true;
    // }

    return max_u64(a_1, a_2) - min_u64(a_1, a_2) < op1->buffer_out.a_sze &&
           max_u64(z_1, z_2) - min_u64(z_1, z_2) < op1->buffer_out.z_sze &&
           max_u64(y_1, y_2) - min_u64(y_1, y_2) < op1->buffer_out.y_sze &&
           max_u64(x_1, x_2) - min_u64(x_1, x_2) < op1->buffer_out.x_sze;
}

static op_group_t op_group_alloc(const linearized_t *linearized, const uint64_t start_idx, uint64_t *op_used) {
    assert(linearized);
    assert(start_idx < linearized->op_len);
    assert(op_used);

    op_group_t group = {0};

    uint64_t op_num = 0;
    for(uint64_t op_off = 1; op_off + start_idx < linearized->op_len; op_off++) {
        if(op_equal(&linearized->op[start_idx], &linearized->op[start_idx + op_off]) &&
           !op_overlaps(&linearized->op[start_idx], &linearized->op[start_idx + op_off])) {

            uint64_t all_same = 1;
            /* No point in the checking inner_off = 0 since that is guaranteed to be true */
            for(uint64_t inner_off = 1; inner_off < op_off; inner_off++) {
                if((!op_equal(&linearized->op[start_idx + inner_off],
                              &linearized->op[start_idx + op_off + inner_off])) ||
                   op_overlaps(&linearized->op[start_idx], &linearized->op[start_idx + op_off])) {
                    all_same = 0;
                    break;
                }
            }
            if(all_same) {
                op_num = op_off;
                break;
            } else {
                continue;
            }
        }
    }
    if(op_num) {
        uint64_t group_num = 1;
        for(uint64_t op_off = op_num; op_off < linearized->op_len - op_num - start_idx + 1; op_off += op_num) {
            /* TODO: Refactor this. There *has* to be a nice way to do this in one loop without having to do a slow
             * modulo */
            uint64_t all_equal = 1;
            for(uint64_t inner_off = 0; inner_off < op_num; inner_off++) {
                if(op_equal(&linearized->op[start_idx + inner_off], &linearized->op[start_idx + op_off + inner_off])) {
                    continue;
                } else {
                    all_equal = 0;
                    break;
                }
            }
            if(all_equal) {
                group_num++;
            } else {
                break;
            }
        }
        group.repeat_num = group_num;
        group.op_num = op_num;
        group.op = calloc(group.op_num, sizeof(op_t));
        for(uint64_t op_off = 0; op_off < op_num; op_off++) {
            group.op[op_off] = linearized->op[start_idx + op_off];
        }
        *op_used += op_num * group_num;
    } else {
        /* Chop 1 op because no repeat was found. Could maybe match more ops but that is bug prone if done naively */
        group.repeat_num = 1;
        group.op_num = 1;
        group.op = calloc(group.op_num, sizeof(op_t));
        group.op[0] = linearized->op[start_idx];
        *op_used += 1;
    }

    /* TODO: Sort this and error or split up til it is possible to map all indices accurately */
    /* TODO: Merge these loops */
    group.dim_info = calloc(group.op_num, sizeof(dim_info_t));
    for(uint64_t op_idx = 0; op_idx < group.op_num; op_idx++) {
        group.dim_info[op_idx].off_out = linearized->op[start_idx + op_idx].buffer_out.off;
        const uint64_t a_initial = linearized->op[start_idx + op_idx].buffer_out.a_off;
        const uint64_t z_initial = linearized->op[start_idx + op_idx].buffer_out.z_off;
        const uint64_t y_initial = linearized->op[start_idx + op_idx].buffer_out.y_off;
        const uint64_t x_initial = linearized->op[start_idx + op_idx].buffer_out.x_off;
        uint64_t a_left = 0;
        uint64_t z_left = 0;
        uint64_t y_left = 0;
        uint64_t x_left = 0;
        uint64_t a_reenter = 0;
        uint64_t z_reenter = 0;
        uint64_t y_reenter = 0;
        uint64_t x_reenter = 0;
        for(uint64_t repeat_idx = 0; repeat_idx < group.repeat_num; repeat_idx++) {
            if(a_left) {
                if(!a_reenter) {
                    if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.a_off == a_initial) {
                        group.dim_info[op_idx].res_a_out = repeat_idx;
                        a_reenter = 1;
                    }
                }
            } else {
                if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.a_off != a_initial) {
                    group.dim_info[op_idx].wai_a_out = repeat_idx;
                    group.dim_info[op_idx].str_a_out =
                        linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.a_off - a_initial;
                    a_left = 1;
                }
            }
            if(z_left) {
                if(!z_reenter) {
                    if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.z_off == z_initial) {
                        group.dim_info[op_idx].res_z_out = repeat_idx;
                        z_reenter = 1;
                    }
                }
            } else {
                if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.z_off != z_initial) {
                    group.dim_info[op_idx].wai_z_out = repeat_idx;
                    group.dim_info[op_idx].str_z_out =
                        linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.z_off - z_initial;
                    z_left = 1;
                }
            }
            if(y_left) {
                if(!y_reenter) {
                    if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.y_off == y_initial) {
                        group.dim_info[op_idx].res_y_out = repeat_idx;
                        y_reenter = 1;
                    }
                }
            } else {
                if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.y_off != y_initial) {
                    group.dim_info[op_idx].wai_y_out = repeat_idx;
                    group.dim_info[op_idx].str_y_out =
                        linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.y_off - y_initial;
                    y_left = 1;
                }
            }
            if(x_left) {
                if(!x_reenter) {
                    if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.x_off == x_initial) {
                        group.dim_info[op_idx].res_x_out = repeat_idx;
                        x_reenter = 1;
                    }
                }
            } else {
                if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.x_off != x_initial) {
                    group.dim_info[op_idx].wai_x_out = repeat_idx;
                    group.dim_info[op_idx].str_x_out =
                        linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_out.x_off - x_initial;
                    x_left = 1;
                }
            }
        }
        if(!a_left) {
            group.dim_info[op_idx].res_a_out = 1;
            group.dim_info[op_idx].str_a_out = 0;
            group.dim_info[op_idx].wai_a_out = 1;
        } else {
            if(!a_reenter) {
                group.dim_info[op_idx].res_a_out = linearized->op_len;
            }
        }
        if(!z_left) {
            group.dim_info[op_idx].res_z_out = 1;
            group.dim_info[op_idx].str_z_out = 0;
            group.dim_info[op_idx].wai_z_out = 1;
        } else {
            if(!z_reenter) {
                group.dim_info[op_idx].res_z_out = linearized->op_len;
            }
        }
        if(!y_left) {
            group.dim_info[op_idx].res_y_out = 1;
            group.dim_info[op_idx].str_y_out = 0;
            group.dim_info[op_idx].wai_y_out = 1;
        } else {
            if(!y_reenter) {
                group.dim_info[op_idx].res_y_out = linearized->op_len;
            }
        }
        if(!x_left) {
            group.dim_info[op_idx].res_x_out = 1;
            group.dim_info[op_idx].str_x_out = 0;
            group.dim_info[op_idx].wai_x_out = 1;
        } else {
            if(!x_reenter) {
                group.dim_info[op_idx].res_x_out = linearized->op_len;
            }
        }
    }
    for(uint64_t op_idx = 0; op_idx < group.op_num; op_idx++) {
        group.dim_info[op_idx].off_in = linearized->op[start_idx + op_idx].buffer_in.off;
        const uint64_t a_initial = linearized->op[start_idx + op_idx].buffer_in.a_off;
        const uint64_t z_initial = linearized->op[start_idx + op_idx].buffer_in.z_off;
        const uint64_t y_initial = linearized->op[start_idx + op_idx].buffer_in.y_off;
        const uint64_t x_initial = linearized->op[start_idx + op_idx].buffer_in.x_off;
        uint64_t a_left = 0;
        uint64_t z_left = 0;
        uint64_t y_left = 0;
        uint64_t x_left = 0;
        uint64_t a_reenter = 0;
        uint64_t z_reenter = 0;
        uint64_t y_reenter = 0;
        uint64_t x_reenter = 0;
        for(uint64_t repeat_idx = 0; repeat_idx < group.repeat_num; repeat_idx++) {
            if(a_left) {
                if(!a_reenter) {
                    if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.a_off == a_initial) {
                        group.dim_info[op_idx].res_a_in = repeat_idx;
                        a_reenter = 1;
                    }
                }
            } else {
                if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.a_off != a_initial) {
                    group.dim_info[op_idx].wai_a_in = repeat_idx;
                    group.dim_info[op_idx].str_a_in =
                        linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.a_off - a_initial;
                    a_left = 1;
                }
            }
            if(z_left) {
                if(!z_reenter) {
                    if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.z_off == z_initial) {
                        group.dim_info[op_idx].res_z_in = repeat_idx;
                        z_reenter = 1;
                    }
                }
            } else {
                if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.z_off != z_initial) {
                    group.dim_info[op_idx].wai_z_in = repeat_idx;
                    group.dim_info[op_idx].str_z_in =
                        linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.z_off - z_initial;
                    z_left = 1;
                }
            }
            if(y_left) {
                if(!y_reenter) {
                    if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.y_off == y_initial) {
                        group.dim_info[op_idx].res_y_in = repeat_idx;
                        y_reenter = 1;
                    }
                }
            } else {
                if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.y_off != y_initial) {
                    group.dim_info[op_idx].wai_y_in = repeat_idx;
                    group.dim_info[op_idx].str_y_in =
                        linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.y_off - y_initial;
                    y_left = 1;
                }
            }
            if(x_left) {
                if(!x_reenter) {
                    if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.x_off == x_initial) {
                        group.dim_info[op_idx].res_x_in = repeat_idx;
                        x_reenter = 1;
                    }
                }
            } else {
                if(linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.x_off != x_initial) {
                    group.dim_info[op_idx].wai_x_in = repeat_idx;
                    group.dim_info[op_idx].str_x_in =
                        linearized->op[start_idx + op_idx + repeat_idx * group.op_num].buffer_in.x_off - x_initial;
                    x_left = 1;
                }
            }
        }
        if(!a_left) {
            group.dim_info[op_idx].res_a_in = 1;
            group.dim_info[op_idx].str_a_in = 0;
            group.dim_info[op_idx].wai_a_in = 1;
        } else {
            if(!a_reenter) {
                group.dim_info[op_idx].res_a_in = linearized->op_len;
            }
        }
        if(!z_left) {
            group.dim_info[op_idx].res_z_in = 1;
            group.dim_info[op_idx].str_z_in = 0;
            group.dim_info[op_idx].wai_z_in = 1;
        } else {
            if(!z_reenter) {
                group.dim_info[op_idx].res_z_in = linearized->op_len;
            }
        }
        if(!y_left) {
            group.dim_info[op_idx].res_y_in = 1;
            group.dim_info[op_idx].str_y_in = 0;
            group.dim_info[op_idx].wai_y_in = 1;
        } else {
            if(!y_reenter) {
                group.dim_info[op_idx].res_y_in = linearized->op_len;
            }
        }
        if(!x_left) {
            group.dim_info[op_idx].res_x_in = 1;
            group.dim_info[op_idx].str_x_in = 0;
            group.dim_info[op_idx].wai_x_in = 1;
        } else {
            if(!x_reenter) {
                group.dim_info[op_idx].res_x_in = linearized->op_len;
            }
        }
    }

    return group;
}
static void op_group_free(op_group_t *group) {
    assert(group);
    if(group->op) {
        free(group->op);
        group->dim_info = NULL;
    }
    if(group->dim_info) {
        free(group->dim_info);
        group->dim_info = NULL;
    }
    group->op_num = 0;
    group->repeat_num = 0;
}
static void op_group_print(op_group_t *group, int padding, int offset, const char *name) {
    if(!strncmp(name, "", 1)) {
        printf("%*s%s len: %lu, repeats: %lu, ops:\n", offset, "", name, group->op_num, group->repeat_num);
    } else {
        printf("%*sop group len: %lu, repeats: %lu, ops:\n", offset, "", group->op_num, group->repeat_num);
    }
    for(uint64_t op_idx = 0; op_idx < group->op_num; op_idx++) {
        printf("%*s[%lu] - ", offset + padding, "", op_idx);
        op_print(&group->op[op_idx], 0, 0, "");
        const dim_info_t dim_info = group->dim_info[op_idx];
        if(group->op[op_idx].type_op == op_unary) {
            printf("%*sa {%lu, %lu, %lu, %lu}\n", offset + 2 * padding, "", dim_info.off_out, dim_info.str_a_out,
                   dim_info.wai_a_out, dim_info.res_a_out);
            printf("%*sz {%lu, %lu, %lu, %lu}\n", offset + 2 * padding, "", dim_info.off_out, dim_info.str_z_out,
                   dim_info.wai_z_out, dim_info.res_z_out);
            printf("%*sy {%lu, %lu, %lu, %lu}\n", offset + 2 * padding, "", dim_info.off_out, dim_info.str_y_out,
                   dim_info.wai_y_out, dim_info.res_y_out);
            printf("%*sx {%lu, %lu, %lu, %lu}\n", offset + 2 * padding, "", dim_info.off_out, dim_info.str_x_out,
                   dim_info.wai_x_out, dim_info.res_x_out);
        } else {
            printf("%*sa {%lu, %lu, %lu, %lu} {%lu, %lu, %lu, %lu}\n", offset + 2 * padding, "", dim_info.off_out,
                   dim_info.str_a_out, dim_info.wai_a_out, dim_info.res_a_out, dim_info.off_in, dim_info.str_a_in,
                   dim_info.wai_a_in, dim_info.res_a_in);
            printf("%*sz {%lu, %lu, %lu, %lu} {%lu, %lu, %lu, %lu}\n", offset + 2 * padding, "", dim_info.off_out,
                   dim_info.str_z_out, dim_info.wai_z_out, dim_info.res_z_out, dim_info.off_in, dim_info.str_z_in,
                   dim_info.wai_z_in, dim_info.res_z_in);
            printf("%*sy {%lu, %lu, %lu, %lu} {%lu, %lu, %lu, %lu}\n", offset + 2 * padding, "", dim_info.off_out,
                   dim_info.str_y_out, dim_info.wai_y_out, dim_info.res_y_out, dim_info.off_in, dim_info.str_y_in,
                   dim_info.wai_y_in, dim_info.res_y_in);
            printf("%*sx {%lu, %lu, %lu, %lu} {%lu, %lu, %lu, %lu}\n", offset + 2 * padding, "", dim_info.off_out,
                   dim_info.str_x_out, dim_info.wai_x_out, dim_info.res_x_out, dim_info.off_in, dim_info.str_x_in,
                   dim_info.wai_x_in, dim_info.res_x_in);
        }
    }
}

const uint64_t arg_cap_min = 1;
static kernel_t kernel_alloc(const op_group_t *group, const uint64_t size_global, const uint64_t size_local,
                             const uint64_t optimizations) {
    kernel_t kernel = {0};

    /* TODO: Measure if using name offsets is faster despite having to generate the strings from the offsets again */
    char **arg_name = calloc(arg_cap_min, sizeof(char *));
    cl_mem *arg_mem = calloc(arg_cap_min, sizeof(cl_mem));
    uint64_t arg_cap = arg_cap_min;
    uint64_t arg_num = 0;
    const uint64_t inlined = optimizations & optimization_inline;
    if(inlined) {
        TODO();
    } else {
        /* Only storing the name offsets is more efficient */
        /* MAYBE: Could make a binary search type thing here */
        for(uint64_t op_idx = 0; op_idx < group->op_num; op_idx++) {
            uint64_t op_found_out = 0;
            for(uint64_t arg_idx = 0; arg_idx < arg_num; arg_idx++) {
                if(!memcmp(arg_name[arg_idx], group->op[op_idx].buffer_out.name, BUFFER_NAME_SIZE)) {
                    op_found_out = 1;
                    break;
                }
            }
            if(!op_found_out) {
                arg_name[arg_num] = calloc(BUFFER_NAME_SIZE + 1, sizeof(char));
                memcpy(arg_name[arg_num], group->op[op_idx].buffer_out.name, BUFFER_NAME_SIZE);
                arg_mem[arg_num] = group->op[op_idx].buffer_out.val_cl;
                arg_num++;
                if(arg_num == arg_cap) {
                    arg_cap *= 2;
                    arg_name = reallocarray(arg_name, arg_cap, sizeof(uint64_t));
                    arg_mem = reallocarray(arg_mem, arg_cap, sizeof(cl_mem));
                }
            }
            /* MAYBE: There might be some trickery to avoid having to run the arg search loop twice */
            if(group->op[op_idx].type_op != op_unary) {
                uint64_t op_found_in = 0;
                for(uint64_t arg_idx = 0; arg_idx < arg_num; arg_idx++) {
                    if(!memcmp(arg_name[arg_idx], group->op[op_idx].buffer_in.name, BUFFER_NAME_SIZE)) {
                        op_found_in = 1;
                        break;
                    }
                }
                if(!op_found_in) {
                    arg_name[arg_num] = calloc(BUFFER_NAME_SIZE + 1, sizeof(char));
                    memcpy(arg_name[arg_num], group->op[op_idx].buffer_in.name, BUFFER_NAME_SIZE);
                    arg_mem[arg_num] = group->op[op_idx].buffer_in.val_cl;
                    arg_num++;
                    if(arg_num == arg_cap) {
                        arg_cap *= 2;
                        arg_name = reallocarray(arg_name, arg_cap, sizeof(uint64_t));
                        arg_mem = reallocarray(arg_mem, arg_cap, sizeof(cl_mem));
                    }
                }
            }
        }
    }

    kernel.arg_name = arg_name;
    kernel.arg_num = arg_num;
    kernel.arg_mem = arg_mem;

    /* TODO: Choose a more accurate name because this just creates the source. The compilation happens when program_run
     * first gets called */
    /* TODO: Maybe make a function in `../runtimes/cl.c` and `../runtimes/cl.h` that just compile the kernel so that it
     * is more clear when what happens? */
    compile_op_group(&kernel, group, size_global, size_local, optimizations);

    return kernel;
}
static void kernel_free(kernel_t *kernel) {
    assert(kernel);
    if(kernel->source) {
        free(kernel->source);
        kernel->source = NULL;
    }
    if(kernel->arg_name) {
        for(uint64_t arg_idx = 0; arg_idx < kernel->arg_num; arg_idx++) {
            if(kernel->arg_name[arg_idx]) {
                free(kernel->arg_name[arg_idx]);
                kernel->arg_name[arg_idx] = NULL;
            }
        }
        free(kernel->arg_name);
        kernel->arg_name = NULL;
    }
    if(kernel->arg_mem) {
        free(kernel->arg_mem);
        kernel->arg_mem = NULL;
    }
    kernel->arg_num = 0;
    kernel->arg_cap = 0;
    if(kernel->cl_kernel) {
        clReleaseKernel(kernel->cl_kernel);
        kernel->cl_kernel = NULL;
    }
    if(kernel->cl_program) {
        clReleaseProgram(kernel->cl_program);
        kernel->cl_program = NULL;
    }
}

/* TODO: Also pass optimization options */
program_t program_compile(const linearized_t *linearized, const cl_device_id *device_id, const cl_context *context,
                          const cl_command_queue *command_queue, const uint64_t global_size,
                          const uint64_t local_size) {
    assert(linearized);
    assert(device_id);
    assert(*device_id);
    assert(context);
    assert(*context);
    assert(command_queue);
    assert(*command_queue);
    assert(global_size);
    assert(local_size);
    assert(local_size <= global_size);

    program_t program = {
        .local_size = local_size,
        .global_size = global_size,
        .kernel = NULL,
        .kernel_cap = 0,
        .kernel_num = 0,
        .cl_context = (cl_context *) context,
        .cl_device_id = (cl_device_id *) device_id,
        .cl_command_queue = (cl_command_queue *) command_queue,
    };

    if(!linearized->op_len) {
        // printf("Empty linearized\n");
        return program;
    }

    uint64_t op_used = 0;
    /* At min each kernel takes one op */
    const uint64_t kernel_max = linearized->op_len;
    program.kernel_cap = 4;
    program.kernel = calloc(program.kernel_cap, sizeof(kernel_t));
    for(uint64_t kernel_idx = 0; kernel_idx < kernel_max; kernel_idx++) {
        op_group_t group = op_group_alloc(linearized, op_used, &op_used);

        program.kernel_num++;
        if(program.kernel_num == program.kernel_cap) {
            program.kernel_cap *= 2;
            program.kernel = reallocarray(program.kernel, program.kernel_cap, sizeof(kernel_t));
            assert(program.kernel);
        }
        program.kernel[program.kernel_num - 1] = kernel_alloc(&group, global_size, local_size, optimization_none);

        op_group_free(&group);

        if(op_used < linearized->op_len) {
            continue;
        } else if(op_used == linearized->op_len) {
            break;
        } else {
            ERROR("Error in kernel extraction in `program_compile()`. Used more ops than there are.\n");
        }
    }
    assert(op_used == linearized->op_len);

    return program;
}
void program_free(program_t *program) {
    if(program->cl_context) {
        if(*program->cl_context) {
            clReleaseContext(*program->cl_context);
            *program->cl_context = NULL;
        }
        program->cl_context = NULL;
    }
    if(program->cl_device_id) {
        if(*program->cl_device_id) {
            clReleaseDevice(*program->cl_device_id);
            *program->cl_device_id = NULL;
        }
        program->cl_device_id = NULL;
    }
    if(program->cl_command_queue) {
        if(*program->cl_command_queue) {
            clReleaseCommandQueue(*program->cl_command_queue);
            *program->cl_command_queue = NULL;
        }
        program->cl_command_queue = NULL;
    }
    for(uint64_t kernel_idx = 0; kernel_idx < program->kernel_num; kernel_idx++) {
        kernel_free(&program->kernel[kernel_idx]);
    }
    free(program->kernel);
    program->kernel = NULL;
    program->kernel_num = 0;
    program->kernel_cap = 0;
    program->local_size = 0;
    program->global_size = 0;
}

/* This only really makes sense if the ops are equal by op_equal */
// static inline bool op_overlaps(const op_t *op1, const op_t *op2) {
//     assert(op1);
//     assert(op2);
//     if(op1->buffer_out.a_off < op2->buffer_out.a_off) {
//         if(op1->buffer_out.a_off + op1->buffer_out.a_sze > op2->buffer_out.a_off) {
//             return true;
//         }
//     } else if(op1->buffer_out.a_off > op2->buffer_out.a_off) {
//         if(op1->buffer_out.a_off > op2->buffer_out.a_off + op2->buffer_out.a_sze) {
//             return true;
//         }
//     }
//     if(op1->buffer_out.z_off < op2->buffer_out.z_off) {
//         if(op1->buffer_out.z_off + op1->buffer_out.z_sze > op2->buffer_out.z_off) {
//             return true;
//         }
//     } else if(op1->buffer_out.z_off > op2->buffer_out.z_off) {
//         if(op1->buffer_out.z_off > op2->buffer_out.z_off + op2->buffer_out.z_sze) {
//             return true;
//         }
//     }
//     if(op1->buffer_out.y_off < op2->buffer_out.y_off) {
//         if(op1->buffer_out.y_off + op1->buffer_out.y_sze > op2->buffer_out.y_off) {
//             return true;
//         }
//     } else if(op1->buffer_out.y_off > op2->buffer_out.y_off) {
//         if(op1->buffer_out.y_off > op2->buffer_out.y_off + op2->buffer_out.y_sze) {
//             return true;
//         }
//     }
//     if(op1->buffer_out.x_off < op2->buffer_out.x_off) {
//         if(op1->buffer_out.x_off + op1->buffer_out.x_sze > op2->buffer_out.x_off) {
//             return true;
//         }
//     } else if(op1->buffer_out.x_off > op2->buffer_out.x_off) {
//         if(op1->buffer_out.x_off > op2->buffer_out.x_off + op2->buffer_out.x_sze) {
//             return true;
//         }
//     }
//     return false;
// }
