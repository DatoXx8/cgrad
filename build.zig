const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});

    const exe = b.addExecutable(.{
        .name = "grads",
        .root_source_file = b.path("src/main.zig"),
        .target = target,
        .optimize = optimize,
    });
    exe.addIncludePath(.{
        .cwd_relative = "/usr/include/",
    });
    // TODO: Figure out how to get rid of libc
    exe.linkSystemLibrary("c");
    exe.linkSystemLibrary("OpenCL");
    b.installArtifact(exe);
    const run_cmd = b.addRunArtifact(exe);
    run_cmd.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        run_cmd.addArgs(args);
    }
    const run_step = b.step("run", "Run the app");
    run_step.dependOn(&run_cmd.step);

    const unit_test_op = b.addExecutable(.{
        .name = "unit_test_op",
        .root_source_file = b.path("src/unit-ops.zig"),
        .target = target,
        .optimize = optimize,
    });
    unit_test_op.addIncludePath(.{
        .cwd_relative = "/usr/include/",
    });
    // TODO: Figure out how to get rid of libc
    unit_test_op.linkSystemLibrary("c");
    unit_test_op.linkSystemLibrary("OpenCL");
    b.installArtifact(unit_test_op);
    const test_op = b.addRunArtifact(unit_test_op);
    test_op.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        test_op.addArgs(args);
    }
    const test_op_step = b.step("test-op", "Run unit tests for singular ops");
    test_op_step.dependOn(&test_op.step);

    const simulation_test_linearized = b.addExecutable(.{
        .name = "simulate-linearized",
        .root_source_file = b.path("src/simulate-linearized.zig"),
        .target = target,
        .optimize = optimize,
    });
    simulation_test_linearized.addIncludePath(.{
        .cwd_relative = "/usr/include/",
    });
    // TODO: Figure out how to get rid of libc
    simulation_test_linearized.linkSystemLibrary("c");
    simulation_test_linearized.linkSystemLibrary("OpenCL");
    b.installArtifact(simulation_test_linearized);
    const test_linearized = b.addRunArtifact(simulation_test_linearized);
    test_linearized.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        test_linearized.addArgs(args);
    }
    const test_linearized_step = b.step("test-linearized", "Run the simulator for linearized ops");
    test_linearized_step.dependOn(&test_linearized.step);

    const simulation_test_compiler = b.addExecutable(.{
        .name = "simulate-compiler",
        .root_source_file = b.path("src/simulate-compiler.zig"),
        .target = target,
        .optimize = optimize,
    });
    simulation_test_compiler.addIncludePath(.{
        .cwd_relative = "/usr/include/",
    });
    // TODO: Figure out how to get rid of libc
    simulation_test_compiler.linkSystemLibrary("c");
    simulation_test_compiler.linkSystemLibrary("OpenCL");
    b.installArtifact(simulation_test_compiler);
    const test_compiler = b.addRunArtifact(simulation_test_compiler);
    test_compiler.step.dependOn(b.getInstallStep());
    if (b.args) |args| {
        test_compiler.addArgs(args);
    }
    const test_compiler_step = b.step("test-compiler", "Run the simulator for the compiler");
    test_compiler_step.dependOn(&test_compiler.step);
}
