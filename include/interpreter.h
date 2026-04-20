#pragma once
#ifndef INTERPRETER_H
#define INTERPRETER_H

typedef struct Interpreter Interpreter;

/**
 * Run interpreter.
 * Returns return code.
 */
int interpreter_run(Interpreter *self);

/**
 * Create a new interpreter.
 * Return NULL if error occured.
 */
Interpreter *interpreter_create(const char *fname);

/**
 * Destroy the interpreter.
 * Do nothing if `self` is NULL.
 */
void interpreter_destroy(Interpreter *self);

#endif
