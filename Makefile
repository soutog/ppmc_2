####################################################
###################### PROJECT #####################
####################################################

# Nome do executável e pastas
EXECUTAVEL  = main
PATHEXEC    = ./bin
PATHSRC     = ./src
PATHTEMP    = ./temp

# Todos os .cpp em src/ viram .o em temp/
SRCS := $(wildcard $(PATHSRC)/*.cpp)
OBJS := $(patsubst $(PATHSRC)/%.cpp,$(PATHTEMP)/%.o,$(SRCS))
DEPS := $(OBJS:.o=.d)

####################################################
#################### CPLEX PATHS ###################
####################################################
# Ajuste se o seu CPLEX estiver em outro lugar
SYSTEM        = x86-64_linux
LIBFORMAT     = static_pic

CPLEXDIR      = /opt/ibm/ILOG/CPLEX_Studio2211/cplex
CONCERTDIR    = /opt/ibm/ILOG/CPLEX_Studio2211/concert

CPLEXLIBDIR   = $(CPLEXDIR)/lib/$(SYSTEM)/$(LIBFORMAT)
CONCERTLIBDIR = $(CONCERTDIR)/lib/$(SYSTEM)/$(LIBFORMAT)

CPLEXINCDIR   = $(CPLEXDIR)/include
CONCERTINCDIR = $(CONCERTDIR)/include

####################################################
##################### COMPILER #####################
####################################################
CPP      = g++
# Flags de compilação (adicione/remova suas -D… aqui)
CXXSTD   = -std=c++17
WARN     = -Wall -Wextra
OPT      = -O3
SAFETY   = -fstack-protector-all -D_FORTIFY_SOURCE=2 -D_GNU_SOURCE
CPLEXDEF = -fPIC -fexceptions -DIL_STD
OPENMP   = -fopenmp

CXXFLAGS = $(CXXSTD) $(WARN) $(OPT) $(SAFETY) $(CPLEXDEF) $(OPENMP) \
           -I$(CPLEXINCDIR) -I$(CONCERTINCDIR) \
           -MMD -MP

# Bibliotecas para linkedição
LDFLAGS  = -L$(CPLEXLIBDIR) -L$(CONCERTLIBDIR)
LDLIBS   = -lilocplex -lcplex -lconcert -lm -lpthread -ldl

####################################################
###################### TARGETS #####################
####################################################
.PHONY: all clean debug release print-cplex

all: dirs $(PATHEXEC)/$(EXECUTAVEL)

# Versões convenientes
release: all
debug: CXXFLAGS := $(filter-out -O3,$(CXXFLAGS)) -O0 -g
debug: clean all

print-cplex:
	@echo "CPLEX include:   $(CPLEXINCDIR)"
	@echo "CPLEX lib:       $(CPLEXLIBDIR)"
	@echo "CONCERT include: $(CONCERTINCDIR)"
	@echo "CONCERT lib:     $(CONCERTLIBDIR)"

dirs:
	@mkdir -p $(PATHEXEC) $(PATHTEMP)

# Regra de compilação: .cpp -> .o (+ .d com dependências)
$(PATHTEMP)/%.o: $(PATHSRC)/%.cpp
	$(CPP) $(CXXFLAGS) -c $< -o $@

# Linka tudo em um único binário
$(PATHEXEC)/$(EXECUTAVEL): $(OBJS)
	$(CPP) $(CXXFLAGS) $(OBJS) $(LDFLAGS) $(LDLIBS) -o $@

# Limpeza
clean:
	rm -rf $(PATHEXEC) $(PATHTEMP)

# Inclui dependências geradas (-MMD -MP)
-include $(DEPS)
