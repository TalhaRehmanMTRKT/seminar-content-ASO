#include <ilcplex/ilocplex.h>
#include<chrono>
#include <iostream>
#include <fstream>
#include <iomanip>
#include <vector>
ILOSTLBEGIN

typedef IloArray<IloNumVarArray> NumVar2D;


struct DieselGenerator {
    double cost_per_mwh;  // Cost ($/MWh)
    double p_min;         // Minimum capacity (MW)
    double p_max;         // Maximum capacity (MW)
};

int
main(int, char**)
{

    IloEnv env;
    IloModel model(env);
#pragma region Microgrid Input Data
    // Time horizon
    const int T = 24;  // hours in one day
    const int M = 1000000; // Big M for binary variable constraints
    
    // Number of loads and diesel generators
    const int numLoads = 2;
    const int numDGs = 2;

    // --------------------------------------------------------------------
    // 1) Grid Purchase Cost ($/MWh) for each hour
    // --------------------------------------------------------------------
    std::vector<double> c_buy = {
        90, 90, 90, 90, 90, 90,
        110, 110, 110, 110, 110, 125,
        125, 125, 125, 125, 125, 125,
        80, 80, 80, 80, 80,80
    };

    // --------------------------------------------------------------------
    // 2) Load Demand (MW) for each load and each hour
    // --------------------------------------------------------------------
    std::vector<std::vector<double>> p_load(numLoads, std::vector<double>{
        169, 175, 179, 171, 181, 172,
            270, 264, 273, 281, 193, 158,
            161, 162, 250, 260, 267, 271,
            284, 167, 128, 134, 144, 150
    });

    // --------------------------------------------------------------------
    // 3) Renewable Generation Forecast (MW) each hour
    // --------------------------------------------------------------------
    std::vector<double> p_res = {
        16, 17, 17, 100, 181, 172,
        270, 264, 273, 281, 193, 158,
        161, 162, 250, 260, 267, 271,
        284, 167, 128, 134, 144, 150
    };

    // --------------------------------------------------------------------
    // 4) Battery Energy Storage System (BESS) Parameters
    // --------------------------------------------------------------------
    const double p_bess_max = 200.0;    // Max charge/discharge (MW)
    const double eta = 0.95;            // Round-trip efficiency (0–1)
    const double soc_min = 0.10;        // Minimum state of charge
    const double soc_max = 0.90;        // Maximum state of charge

    // --------------------------------------------------------------------
    // 5) Diesel Generator (DG) Parameters - Two Generators
    // --------------------------------------------------------------------
    std::vector<DieselGenerator> dgs = {
        {80.0, 50.0, 200.0},   // DG1 cost , min, max
		{70.0, 50.0, 200.0}    // DG2 cost , min, max
    };

    // --------------------------------------------------------------------
    // Print Summary of All Inputs
    // --------------------------------------------------------------------
    std::cout << std::fixed << std::setprecision(1);
    std::cout << "=== Microgrid Input Data ===\n\n";

    // 1) Grid Purchase Cost
    std::cout << "1) Grid Purchase Cost ($/MWh):\n  Hour :";
    for (int t = 0; t < T; ++t) std::cout << std::setw(6) << t;
    std::cout << "\n  Cost :";
    for (double v : c_buy) std::cout << std::setw(6) << v;
    std::cout << "\n\n";

    // 2) Load Demand Profiles
    for (int i = 0; i < numLoads; ++i) {
        std::cout << "2." << (i + 1) << ") Load " << (i + 1) << " Demand (MW):\n  Hour :";
        for (int t = 0; t < T; ++t) std::cout << std::setw(6) << t;
        std::cout << "\n  Demand:";
        for (double v : p_load[i]) std::cout << std::setw(6) << v;
        std::cout << "\n\n";
    }

    // 3) Renewable Generation
    std::cout << "3) Renewable Generation Forecast (MW):\n  Hour :";
    for (int t = 0; t < T; ++t) std::cout << std::setw(6) << t;
    std::cout << "\n  Gen   :";
    for (double v : p_res) std::cout << std::setw(6) << v;
    std::cout << "\n\n";

    // 4) BESS Parameters
    std::cout << "4) BESS Parameters:\n"
        << "  Max Power (MW)   : " << p_bess_max << "\n"
        << "  Efficiency       : " << eta * 100 << " %\n"
        << "  SoC Range        : [" << soc_min * 100 << "%, " << soc_max * 100 << "%]\n\n";

    // 5) Diesel Generator Parameters
    std::cout << "5) Diesel Generator Parameters:\n";
    for (size_t i = 0; i < dgs.size(); ++i) {
        std::cout << "  DG" << i + 1 << " - Cost: $" << dgs[i].cost_per_mwh << "/MWh"
            << ", Min: " << dgs[i].p_min << " MW"
            << ", Max: " << dgs[i].p_max << " MW\n";
    }
    std::cout << "\n";
#pragma endregion

#pragma region Decision Variables
    NumVar2D p_dg(env, numDGs);
    for (int i = 0; i < numDGs; ++i) {
		p_dg[i] = IloNumVarArray(env, T, 0, IloInfinity, ILOFLOAT);
	}
    IloNumVarArray p_buy(env, T, 0, IloInfinity, ILOFLOAT);//Grid power buy
    IloNumVarArray p_sell(env, T, 0, IloInfinity, ILOFLOAT);//Grid power sell
    IloNumVarArray p_bess_chg(env, T, 0, IloInfinity, ILOFLOAT);//Battery charging power
    IloNumVarArray p_bess_dch(env, T, 0, IloInfinity, ILOFLOAT);//Battery discharging power
    IloNumVarArray soc(env, T, 0, 1, ILOFLOAT);//Battery state of charge
    IloNumVarArray omega(env, T, 0, IloInfinity, ILOINT);//Binary variable 
#pragma endregion

#pragma region Objective Function
    IloExpr objective(env); //Defining the expression for the objective function
    for (int t = 0; t < T; t++)
    {
        objective += c_buy[t] * p_buy[t] - 0.8 * c_buy[t] * p_sell[t] ; //Grid cost
        for (int i = 0; i < numDGs; ++i) {
			objective += dgs[i].cost_per_mwh * p_dg[i][t]; //Diesel generator cost
		}
    }
    // Objective: minimize cost over 24 hours
    model.add(IloMinimize(env, objective));
#pragma endregion


#pragma region Constraints

    for (int t = 0; t < T; t++)
    {

        // Energy balance constraint
        model.add(p_buy[t] + p_res[t] + p_dg[0][t] + p_dg[1][t] + p_bess_dch[t] == p_bess_chg[t] + p_load[0][t] + p_load[1][t] + p_sell[t]);

		// BESS constraints
        if (t == 0) {
			model.add(soc[t] == soc[T-1] + (p_bess_chg[t] * eta - p_bess_dch[t] / eta) / (p_bess_max));
            model.add(p_bess_chg[t] <= p_bess_max * (1 - soc[T - 1]) / eta); // Charge limit 
            model.add(p_bess_dch[t] <= p_bess_max * soc[T - 1] * eta); // Discharge limit
		}
        else {
            model.add(soc[t] == soc[t - 1] + (p_bess_chg[t] * eta - p_bess_dch[t] / eta) / (p_bess_max));
            model.add(p_bess_chg[t] <= p_bess_max * (1 - soc[t - 1]) / eta); // Charge limit
            model.add(p_bess_dch[t] <= p_bess_max * soc[t - 1] * eta); // Discharge limit
		}

        // Binary variable constraints
        model.add(p_bess_chg[t] <= omega[t] * M); // Charge limit
        model.add(p_bess_dch[t] <= (1 - omega[t]) * M); // Discharge limit

        // State of Charge (SoC) limits
		model.add(soc[t] >= soc_min);
		model.add(soc[t] <= soc_max);

        //Diesel Generator constraints
        for (int i = 0; i < numDGs; ++i) {
            model.add(p_dg[i][t] >= dgs[i].p_min);
            model.add(p_dg[i][t] <= dgs[i].p_max);
        }


    }


 #pragma endregion
 
#pragma region Results

    IloCplex cplex(env);
	cplex.extract(model);
	//cplex.setOut(env.getNullStream());
    if (!cplex.solve()) {
		env.error() << "Failed" << endl;
		throw(-1);
	}
	double obj = cplex.getObjValue();
	std::cout << "Minimized Objective Funtion : " << obj << std::endl;
    std::ofstream resultFile("results.csv");

    if (resultFile.is_open()) {
        // Write CSV header
        resultFile << "Hour,Grid_Buy(MW),Grid_Sell(MW)";
        for (int i = 0; i < numDGs; ++i) {
            resultFile << ",DG" << i + 1 << "_Power(MW)";
        }
        // adde load, renewable generation, and BESS data
        resultFile << ",Load1(MW),Load2(MW),Renewable_Gen(MW),BESS_Charge(MW),BESS_Discharge(MW),SoC";
        resultFile << "\n";

        // Write data rows
        for (int t = 0; t < T; ++t) {
            resultFile << t << "," << cplex.getValue(p_buy[t]) << "," << cplex.getValue(p_sell[t]);

            for (int i = 0; i < numDGs; ++i) {
                resultFile << "," << cplex.getValue(p_dg[i][t]);
            }
            // add load, renewable generation, and BESS data
            resultFile << "," << p_load[0][t] << "," << p_load[1][t] << "," << p_res[t] << "," << cplex.getValue(p_bess_chg[t]) << "," << cplex.getValue(p_bess_dch[t]) << "," << cplex.getValue(soc[t]);

            resultFile << "\n";
        }

        resultFile.close();
        std::cout << "Results saved to results.csv\n";
    }
    else {
        std::cerr << "Unable to open results.csv file.\n";
    }

#pragma endregion


    return 0;
}