const std = @import("std");

const Tensor = @import("./tensor.zig").Tensor;
const OpType = @import("./tensor.zig").Op.Type;

const Pcg = @import("./prng.zig").Pcg;

const assert = @import("./util.zig").assert;

const Program = @import("./compiler/program.zig").Program;
const ClContext = @import("./runtimes/cl.zig").ClContext;
const ClDevice = @import("./runtimes/cl.zig").ClDevice;
const ClCommandQueue = @import("./runtimes/cl.zig").ClCommandQueue;

// TODO: Move tests to seperate directory
// TODO: Also randomize random optimization once those are implemented

const AssertError = error{
    nan,
    difference,
};
/// Margin of error
const epsilon: f32 = 1e-6;
/// Check for equality between the two floats within the margin of error of `epsilon`
fn assert_eq(val1: f32, val2: f32) !void {
    if (std.math.approxEqAbs(f32, val1, val2, epsilon)) {
        return;
    } else {
        if (std.math.isNan(val1) or std.math.isNan(val2)) {
            // For nicer output formatting
            std.debug.print("\n", .{});
            std.log.err("Found NaN in equality comparison.\n", .{});
            return AssertError.nan;
        } else {
            // For nicer output formatting
            std.debug.print("\n", .{});
            std.log.err("Difference between {d} and {d} is too large.\n", .{ val1, val2 });
            return AssertError.difference;
        }
    }
}

fn simulate_compiler(allocator: anytype, tensor_num: u32, op_num: u32, op_off: u32, rng: u64, device: ClDevice, context: ClContext, command_queue: ClCommandQueue) !void {
    assert(tensor_num > 1);
    assert(op_num > 0);
    assert(op_num > op_off);
    // const a_size: u32 = 6;
    // const z_size: u32 = 5;
    // const y_size: u32 = 4;
    // const x_size: u32 = 3;
    const a_size: u32 = 2;
    const z_size: u32 = 2;
    const y_size: u32 = 2;
    const x_size: u32 = 2;

    // Arbitrary start points
    var tensor_out: u32 = 0;
    var tensor_in: u32 = 1;
    tensor_out = tensor_out;
    tensor_in = tensor_in;

    // TODO: Random offsets, because like this unary_set makes it kinda uninteresting

    var tensor1: []Tensor = try allocator.alloc(Tensor, tensor_num);
    var tensor2: []Tensor = try allocator.alloc(Tensor, tensor_num);
    defer allocator.free(tensor1);
    defer allocator.free(tensor2);

    for (0..tensor_num) |tensor_idx| {
        tensor1[tensor_idx] = try Tensor.alloc(allocator, a_size, z_size, y_size, x_size, context);
        tensor2[tensor_idx] = try Tensor.alloc(allocator, a_size, z_size, y_size, x_size, context);
    }
    defer {
        for (0..tensor_num) |tensor_idx| {
            tensor1[tensor_idx].free(allocator);
            tensor2[tensor_idx].free(allocator);
        }
    }

    Pcg.init(rng);
    std.debug.print("simulate-compiler: rng={}...", .{rng});

    for (0..tensor_num) |tensor_idx| {
        for (0..a_size * z_size * y_size * x_size) |arg_idx| {
            tensor1[tensor_idx].buffer.values[arg_idx] = Pcg.rand_f32();
            tensor2[tensor_idx].buffer.values[arg_idx] = tensor1[tensor_idx].buffer.values[arg_idx];
        }
    }

    const op_type_max: u32 = @typeInfo(OpType).Enum.fields.len;
    for (0..op_num) |op_idx| {
        const op_type: OpType = @enumFromInt(Pcg.rand_below(op_type_max));

        // The likelyhood is strictly speaking 1 / switch_likelyhood
        const switch_likelyhood: u32 = 10;
        if (Pcg.rand_below(switch_likelyhood) == 0) {
            tensor_in = tensor_out;
            while (tensor_out == tensor_in) {
                tensor_out = Pcg.rand_below(tensor_num);
            }
        }

        // For minifying
        if (op_idx < op_off) {
            // Need to keep prng state consistent between skipping and not skipping
            switch (op_type) {
                .unary_add => {
                    _ = Pcg.rand_f32();
                },
                .unary_subtract => {
                    _ = Pcg.rand_f32();
                },
                .unary_multiply => {
                    _ = Pcg.rand_f32();
                },
                .unary_divide => {
                    _ = Pcg.rand_f32();
                },
                .unary_max => {
                    _ = Pcg.rand_f32();
                },
                .unary_min => {
                    _ = Pcg.rand_f32();
                },
                .unary_set => {
                    _ = Pcg.rand_f32();
                },
                .unary_random => {
                    _ = Pcg.rand_f32();
                },
                else => {},
            }
        } else {
            switch (op_type) {
                .unary_add => {
                    const u_var: f32 = Pcg.rand_f32();
                    try tensor1[tensor_out].unary_add(allocator, u_var);
                    try tensor2[tensor_out].unary_add(allocator, u_var);
                },
                .unary_subtract => {
                    const u_var: f32 = Pcg.rand_f32();
                    try tensor1[tensor_out].unary_subtract(allocator, u_var);
                    try tensor2[tensor_out].unary_subtract(allocator, u_var);
                },
                .unary_multiply => {
                    const u_var: f32 = Pcg.rand_f32();
                    try tensor1[tensor_out].unary_multiply(allocator, u_var);
                    try tensor2[tensor_out].unary_multiply(allocator, u_var);
                },
                .unary_divide => {
                    // NaN prevention
                    const u_var: f32 = @abs(Pcg.rand_f32()) + 1;
                    try tensor1[tensor_out].unary_divide(allocator, u_var);
                    try tensor2[tensor_out].unary_divide(allocator, u_var);
                },
                .unary_exp => {
                    // NaN prevention
                    try tensor1[tensor_out].unary_min(allocator, 10);
                    try tensor2[tensor_out].unary_min(allocator, 10);

                    try tensor1[tensor_out].unary_exp(allocator);
                    try tensor2[tensor_out].unary_exp(allocator);
                },
                .unary_log => {
                    // NaN prevention
                    try tensor1[tensor_out].unary_absolute(allocator);
                    try tensor2[tensor_out].unary_absolute(allocator);
                    try tensor1[tensor_out].unary_add(allocator, 1);
                    try tensor2[tensor_out].unary_add(allocator, 1);

                    try tensor1[tensor_out].unary_log(allocator);
                    try tensor2[tensor_out].unary_log(allocator);
                },
                .unary_square => {
                    // NaN prevention
                    try tensor1[tensor_out].unary_min(allocator, 100);
                    try tensor2[tensor_out].unary_min(allocator, 100);
                    try tensor1[tensor_out].unary_max(allocator, -100);
                    try tensor2[tensor_out].unary_max(allocator, -100);

                    try tensor1[tensor_out].unary_square(allocator);
                    try tensor2[tensor_out].unary_square(allocator);
                },
                .unary_sqrt => {
                    // NaN prevention
                    try tensor1[tensor_out].unary_absolute(allocator);
                    try tensor2[tensor_out].unary_absolute(allocator);

                    try tensor1[tensor_out].unary_sqrt(allocator);
                    try tensor2[tensor_out].unary_sqrt(allocator);
                },
                .unary_reciprocal => {
                    // NaN prevention
                    try tensor1[tensor_out].unary_absolute(allocator);
                    try tensor2[tensor_out].unary_absolute(allocator);
                    try tensor1[tensor_out].unary_add(allocator, 1);
                    try tensor2[tensor_out].unary_add(allocator, 1);

                    try tensor1[tensor_out].unary_reciprocal(allocator);
                    try tensor2[tensor_out].unary_reciprocal(allocator);
                },
                .unary_max => {
                    const u_var: f32 = Pcg.rand_f32();
                    try tensor1[tensor_out].unary_max(allocator, u_var);
                    try tensor2[tensor_out].unary_max(allocator, u_var);
                },
                .unary_min => {
                    const u_var: f32 = Pcg.rand_f32();
                    try tensor1[tensor_out].unary_min(allocator, u_var);
                    try tensor2[tensor_out].unary_min(allocator, u_var);
                },
                .unary_set => {
                    const u_var: f32 = Pcg.rand_f32();
                    try tensor1[tensor_out].unary_set(allocator, u_var);
                    try tensor2[tensor_out].unary_set(allocator, u_var);
                },
                .unary_random => {
                    // Not doing this because I would have to reset the rng
                    const u_var: f32 = Pcg.rand_f32();
                    try tensor1[tensor_out].unary_set(allocator, u_var);
                    try tensor2[tensor_out].unary_set(allocator, u_var);
                },
                .unary_tanh => {
                    try tensor1[tensor_out].unary_tanh(allocator);
                    try tensor2[tensor_out].unary_tanh(allocator);
                },
                .unary_absolute => {
                    try tensor1[tensor_out].unary_absolute(allocator);
                    try tensor2[tensor_out].unary_absolute(allocator);
                },
                .unary_sign => {
                    try tensor1[tensor_out].unary_sign(allocator);
                    try tensor2[tensor_out].unary_sign(allocator);
                },
                .binary_add => {
                    try tensor1[tensor_out].binary_add(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].binary_add(allocator, &tensor2[tensor_in]);
                },
                .binary_subtract => {
                    try tensor1[tensor_out].binary_subtract(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].binary_subtract(allocator, &tensor2[tensor_in]);
                },
                .binary_multiply => {
                    try tensor1[tensor_out].binary_multiply(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].binary_multiply(allocator, &tensor2[tensor_in]);
                },
                .binary_divide => {
                    // NaN prevention
                    try tensor1[tensor_in].unary_absolute(allocator);
                    try tensor2[tensor_in].unary_absolute(allocator);
                    try tensor1[tensor_in].unary_add(allocator, 1);
                    try tensor2[tensor_in].unary_add(allocator, 1);

                    try tensor1[tensor_out].binary_divide(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].binary_divide(allocator, &tensor2[tensor_in]);
                },
                .binary_max => {
                    try tensor1[tensor_out].binary_max(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].binary_max(allocator, &tensor2[tensor_in]);
                },
                .binary_min => {
                    try tensor1[tensor_out].binary_min(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].binary_min(allocator, &tensor2[tensor_in]);
                },
                .binary_set => {
                    try tensor1[tensor_out].binary_set(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].binary_set(allocator, &tensor2[tensor_in]);
                },
                .linary_add => {
                    tensor1[tensor_in].move_resize(1, 1, 1, 1);
                    tensor2[tensor_in].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].linary_add(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].linary_add(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                },
                .linary_subtract => {
                    tensor1[tensor_in].move_resize(1, 1, 1, 1);
                    tensor2[tensor_in].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].linary_subtract(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].linary_subtract(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                },
                .linary_multiply => {
                    tensor1[tensor_in].move_resize(1, 1, 1, 1);
                    tensor2[tensor_in].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].linary_multiply(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].linary_multiply(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                },
                .linary_divide => {
                    tensor1[tensor_in].move_resize(1, 1, 1, 1);
                    tensor2[tensor_in].move_resize(1, 1, 1, 1);
                    // NaN prevention
                    try tensor1[tensor_in].unary_absolute(allocator);
                    try tensor2[tensor_in].unary_absolute(allocator);
                    try tensor1[tensor_in].unary_add(allocator, 1);
                    try tensor2[tensor_in].unary_add(allocator, 1);

                    try tensor1[tensor_out].linary_divide(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].linary_divide(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                },
                .linary_max => {
                    tensor1[tensor_in].move_resize(1, 1, 1, 1);
                    tensor2[tensor_in].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].linary_max(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].linary_max(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                },
                .linary_min => {
                    tensor1[tensor_in].move_resize(1, 1, 1, 1);
                    tensor2[tensor_in].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].linary_min(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].linary_min(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                },
                .linary_set => {
                    tensor1[tensor_in].move_resize(1, 1, 1, 1);
                    tensor2[tensor_in].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].linary_set(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].linary_set(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_in].move_resize(a_size, z_size, y_size, x_size);
                },
                .reduce_sum => {
                    tensor1[tensor_out].move_resize(1, 1, 1, 1);
                    tensor2[tensor_out].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].reduce_sum(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].reduce_sum(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_out].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_out].move_resize(a_size, z_size, y_size, x_size);
                },
                .reduce_max => {
                    tensor1[tensor_out].move_resize(1, 1, 1, 1);
                    tensor2[tensor_out].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].reduce_max(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].reduce_max(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_out].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_out].move_resize(a_size, z_size, y_size, x_size);
                },
                .reduce_min => {
                    tensor1[tensor_out].move_resize(1, 1, 1, 1);
                    tensor2[tensor_out].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].reduce_min(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].reduce_min(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_out].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_out].move_resize(a_size, z_size, y_size, x_size);
                },
                .reduce_avg => {
                    tensor1[tensor_out].move_resize(1, 1, 1, 1);
                    tensor2[tensor_out].move_resize(1, 1, 1, 1);
                    try tensor1[tensor_out].reduce_avg(allocator, &tensor1[tensor_in]);
                    try tensor2[tensor_out].reduce_avg(allocator, &tensor2[tensor_in]);
                    tensor1[tensor_out].move_resize(a_size, z_size, y_size, x_size);
                    tensor2[tensor_out].move_resize(a_size, z_size, y_size, x_size);
                },
            }
        }
    }

    tensor2[tensor_out].realize();

    const size_global: usize = 20;
    const size_local: usize = 4;

    for (0..tensor_num) |tensor_idx| {
        tensor1[tensor_idx].buffer.sync_update(.sync_to_device);
        try tensor1[tensor_idx].buffer.sync_to_device(command_queue);
    }
    const program: Program = try Program.alloc(allocator, tensor1[tensor_out].linearized, size_global, size_local, device, context, command_queue);
    try program.run();
    try program.free(allocator);
    for (0..tensor_num) |tensor_idx| {
        tensor1[tensor_idx].buffer.sync_update(.sync_to_host);
        try tensor1[tensor_idx].buffer.sync_to_host(command_queue);
    }

    for (0..a_size * z_size * y_size * x_size) |arg_idx| {
        try assert_eq(tensor1[tensor_out].buffer.values[arg_idx], tensor2[tensor_out].buffer.values[arg_idx]);
    }

    std.debug.print(" passed!\n", .{});
}

fn minify_compiler(allocator: anytype, tensor_num: u32, op_num: u32, rng: u64, err: anytype, device: ClDevice, context: ClContext, command_queue: ClCommandQueue) !void {
    // TODO: Assert that the thing actually fails
    assert(tensor_num > 1);
    assert(op_num > 0);
    var op_top: u32 = 1;
    for (0..op_num) |op_removed| {
        var failed: bool = false;
        simulate_compiler(allocator, tensor_num, @truncate(op_num - op_removed), 0, rng, device, context, command_queue) catch {
            failed = true;
        };
        if (failed) {
            continue;
        } else {
            op_top = @truncate(op_num - op_removed + 1);
            break;
        }
    }
    var op_low: u32 = op_top - 1;
    for (0..op_top) |op_removed| {
        var failed: bool = false;
        simulate_compiler(allocator, tensor_num, op_top, @truncate(op_removed), rng, device, context, command_queue) catch {
            failed = true;
        };
        if (failed) {
            continue;
        } else {
            op_low = @truncate(op_removed);
            break;
        }
    }
    std.debug.print("Failed at ops {}-{}\n", .{ op_low, op_top });
    return err;
}

pub fn main() !void {
    var gpa = std.heap.GeneralPurposeAllocator(.{}){};
    const allocator = gpa.allocator();
    defer _ = gpa.detectLeaks();

    var args = try std.process.argsWithAllocator(allocator);
    defer args.deinit();

    var rng_saved: ?u64 = null;
    var loop_infinite: bool = false;
    var loop_count: u64 = 1;
    // Skip the executable call
    _ = args.next();
    if (args.next()) |arg| {
        if (std.mem.startsWith(u8, arg, "rng=")) {
            const offset = "rng="[0..].len;
            rng_saved = try std.fmt.parseInt(u64, arg[offset..], 10);
        } else if (std.mem.startsWith(u8, arg, "loop=")) {
            const offset = "loop="[0..].len;
            loop_count = std.fmt.parseInt(u64, arg[offset..], 10) catch 0;
            if (loop_count == 0) {
                loop_infinite = true;
            }
            // Iff the loop is infinite then the loop count has to be 0
            assert(loop_infinite == (loop_count == 0));
        } else {
            std.log.err("Found unrecognised option `{s}`, expected `rng=<number>` or `loop=[number].\n", .{arg});
            assert(false);
        }
    }
    const rng: u64 = switch (rng_saved == null) {
        true => @bitCast(std.time.microTimestamp()),
        false => rng_saved.?,
    };

    const tensor_num: u32 = 10;
    const op_num: u32 = 1;
    comptime {
        assert(tensor_num > 1);
        assert(op_num > 0);
    }

    const device: ClDevice = try ClDevice.alloc(.Gpu);
    const context: ClContext = try ClContext.alloc(device);
    const command_queue: ClCommandQueue = try ClCommandQueue.alloc(device, context);

    if (loop_infinite) {
        var loop_idx: u64 = 0;
        // TODO: Decide how to reseed the random number generator here...
        // rng + loop_idx "wastes" the least seeds but it could cause issues
        // when running multiple threads with this because you then run the same tests over and over again
        while (true) {
            std.debug.print("{} => ", .{loop_idx});
            simulate_compiler(allocator, tensor_num, op_num, 0, rng + loop_idx, device, context, command_queue) catch |err| {
                try minify_compiler(allocator, tensor_num, op_num, rng, err, device, context, command_queue);
            };
            loop_idx += 1;
        }
    } else {
        for (0..loop_count) |loop_idx| {
            std.debug.print("{} => ", .{loop_idx});
            simulate_compiler(allocator, tensor_num, op_num, 0, rng + loop_idx, device, context, command_queue) catch |err| {
                try minify_compiler(allocator, tensor_num, op_num, rng + loop_idx, err, device, context, command_queue);
            };
        }
    }
}