#include <stdio.h>
#include <stdlib.h>

#include "mpc.h"

#ifdef _WIN32
#include <string.h>

static char buffer[2048];

char* readline(char* prompt)
{
	fputs(prompt, stdout);
	fgets(buffer, 2048, stdin);
	char* cpy = malloc(strlen(buffer)+1);
	strcpy(cpy, buffer);
	cpy[strlen(cpy)-1] = '\0';
	return cpy;
}

void add_history(char* unused){}

#else
#include <editline/readline.h>
#include <editline/history.h>
#endif

enum {LVAL_ERR, LVAL_NUM, LVAL_SYM, LVAL_SEXPR };

typedef struct {
	int type;
	double num;
	
	char* err;
	char* sym;
	
	int count;
	struct lval** cell;
	
} lval;

lval* lval_num(double x)
{
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_NUM;
	v->num  = x;
	return v;
}

lval* lval_err(char* msg)
{
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_ERR;
	v->err = malloc(strlen(msg)+1);
	strcpy(v->err, msg);
	return v;
}

lval* lval_sym(char* s)
{
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SYM;
	v->sym = malloc(strlen(s)+1);
	strcpy(v->sym, s);
	return v;
}

lval* lval_sexpr(void)
{
	lval* v = malloc(sizeof(lval));
	v->type = LVAL_SEXPR;
	v->count = 0;
	v->cell = NULL;
	return v;
}

void lval_del(lval* v)
{
	switch(v->type)
	{
		case LVAL_NUM: break;
		
		case LVAL_ERR: free(v->err); break;
		case LVAL_SYM: free(v->sym); break;
		
		case LVAL_SEXPR:
			for(int i = 0; i < v->count; i++)
			{
				lval_del(v->cell[i]);
			}
			
			free(v->cell);
			break;
		
	}
	
	free(v);
}

lval* lval_read_num(mpc_ast_t* t)
{
	errno = 0;
	double x = strtod(t->contents, NULL);
	return errno != ERANGE ? lval_num(x) : lval_err("invalid number");
}

lval* lval_add(lval* v, lval* x)
{
	v->count++;
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	v->cell[v->count-1] = x;
	return v;
}

lval* lval_read(mpc_ast_t* t)
{
	if(strstr(t->tag, "number")) { return lval_read_num(t); }
	if(strstr(t->tag, "symbol")) { return lval_sym(t->contents); }
	
	lval* x = NULL;
	
	if(strcmp(t->tag, ">") == 0) { x = lval_sexpr(); }
	if(strstr(t->tag, "sexpr"))  { x = lval_sexpr(); }
	
	for(int i = 0; i < t->children_num; i++) 
	{
    	if (strcmp(t->children[i]->contents, "(") == 0) { continue; }
    	if (strcmp(t->children[i]->contents, ")") == 0) { continue; }
    	if (strcmp(t->children[i]->contents, "}") == 0) { continue; }
		if (strcmp(t->children[i]->contents, "{") == 0) { continue; }
 		if (strcmp(t->children[i]->tag,  "regex") == 0) { continue; }
		 x = lval_add(x, lval_read(t->children[i]));
  }
  
  return x;
	
}

void lval_print(lval* v);

void lval_expr_print(lval* v, char open, char close)
{
	putchar(open);
	
	for( int i = 0; i < v->count; i++)
	{
		lval_print(v->cell[i]);
		if(i != (v->count - 1))
		{
			putchar(32);
		}
	}
	
	putchar(close);
}

void lval_print(lval* v)
{
	switch(v->type)
	{
		case LVAL_NUM: printf("%f", v->num); break;
		case LVAL_ERR: printf("%s", v->err); break;
		case LVAL_SYM: printf("%s", v->sym); break;
		case LVAL_SEXPR: lval_expr_print(v,'(',')'); break;				
	}
}



void lval_println(lval* v)
{
	lval_print(v); 
	putchar('\n');
}



lval* lval_pop(lval* v, int i)
{
	lval* x  = v->cell[i];
	
	memmove(&v->cell[i], &v->cell[i+1], sizeof(lval*) * (v->count - i - 1));
	
	v->count--;
	
	v->cell = realloc(v->cell, sizeof(lval*) * v->count);
	
	return x;
	
}

lval* lval_take(lval* v, int i)
{
	lval* x = lval_pop(v, i);
	lval_del(v);
	
	return x;
	
}

lval * bulitin_op(lval* a, char* op)
{
	for(int i = 0; i < a->count; i++)
	{
		lval* tmp = a->cell[i];
		if(tmp->type != LVAL_NUM)
		{
			lval_del(a);
			return lval_err("Cannot operate on non-number");
		}
	}
	
	lval* x = lval_pop(a, 0);
	
	if((strcmp(op, "-") == 0) && a->count == 0)
	{
		x->num = - x->num;
	}
	
	while(a->count > 0)
	{
		lval* y = lval_pop(a, 0);
		
		if(strcmp(op, "+") == 0) { x->num += y->num; }
		if (strcmp(op, "-") == 0) { x->num -= y->num; }
		if (strcmp(op, "*") == 0) { x->num *= y->num; }
		if (strcmp(op, "/") == 0) 
		{
			if (y->num == 0) 
			{
				lval_del(x); lval_del(y);
				x = lval_err("Division By Zero!"); break;
			}
				x->num /= y->num;
		}
		
		lval_del(y);
		
	}
	
	lval_del(a);
	
	return x;
}

lval* lval_eval_sexpr(lval* v);

lval* lval_eval(lval* v)
{
	if(v->type == LVAL_SEXPR) 
	{
		return lval_eval_sexpr(v);
	}
	
	return v;
}

lval* lval_eval_sexpr(lval* v)
{
	for(int i = 0; i < v->count; i++)
	{
		v->cell[i] = lval_eval(v->cell[i]);
	}
	
	/* Error checking */
	for(int i = 0; i < v->count; i++)
	{
		lval* tmp = v->cell[i]; /* work around for 'error: dereferencing pointer to incomplete type' */ 
		if(tmp->type == LVAL_ERR) 
		{
			return lval_take(v, i);
		}
	}
	
	/* Empty expr */
	if(v->count == 0)
	{
		return v;
	}
	
	/* Single expr */
	if(v->count == 1) 
	{
		return lval_take(v, 0);
	}
	
	/* Ensure first element is a symbol */
	lval* f = lval_pop(v, 0);
	if(f->type != LVAL_SYM)
	{
		lval_del(f);
		lval_del(v);
		return lval_err("S-expr does not start with symbol");
	} 
	
	lval* result = bulitin_op(v, f->sym);
	lval_del(f);
	return result;
	
}



int main (int argc, char** argv)
{
	mpc_parser_t* Number   = mpc_new("number");
	mpc_parser_t* Symbol   = mpc_new("symbol");
	mpc_parser_t* Sexpr    = mpc_new("sexpr");
	mpc_parser_t* Expr     = mpc_new("expr");
	mpc_parser_t* Lispy    = mpc_new("lispy");
	
	
	mpca_lang(MPCA_LANG_DEFAULT,
		" \
		number   : /-?[0-9]+\\.?[0-9]*/ ; \
		symbol   : '+' | '-' | '*' | '/' ; \
		sexpr    : '(' <expr>* ')' ; \
		expr     : <number> | <symbol> | <sexpr> ; \
		lispy    : /^/ <expr>* /$/ ; \
		",
		Number, Symbol, Sexpr, Expr, Lispy
	);
	
	puts("Lispy Version 0.0.0.0.5");
	puts("Press Ctrl + c to Exit\n");
	
	
	while(1)
	{
		char* input = readline("lispy> ");
		add_history(input);
		
		mpc_result_t r;
		
		if(mpc_parse("<stdin>", input, Lispy, &r))
		{
			/*mpc_ast_print(r.output);
			mpc_ast_delete(r.output);*/
			
			lval* x = lval_read(r.output);
			x = lval_eval(x);
			
			lval_println(x);
			lval_del(x);
			
		}
		else
		{
			mpc_err_print(r.error);
			mpc_err_delete(r.error);
		}
		
		free(input);
	}
	
	mpc_cleanup(5, Number, Symbol, Sexpr, Expr, Lispy);

	return 0;    
}
