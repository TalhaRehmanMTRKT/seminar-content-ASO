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

    // --------------------------------------------
    // A) General Constants and System Parameters
    // --------------------------------------------
    const int T = 24;             // Time horizon (hours in one day)
    const int M = 1000000;        // Big M for binary variable constraints

    const int numLoads = 2;       // Number of loads
    const int numDGs = 2;         // Number of diesel generators
    const int numBuses = 4;       // Number of buses
    const int numBESS = 1;        // Number of battery energy storage systems
    const int numRES = 1;         // Number of renewable energy sources

    const double price_scale = 0.8; // Price scaling factor for selling power

    const bool consider_opf = false; // Consider optimal power flow (OPF) or not

    const int theta_min = -360;   // Minimum angle (degrees)
    const int theta_max = 360;    // Maximum angle (degrees)

    // --------------------------------------------
    // B) Line Parameters and Network Topology
    // --------------------------------------------
    const double x = 0.1;              // Reactance of the line (ohm)
    const double r = 0.0;             // Resistance of the line (ohm)
    const double line_capacity = 500.0;  // Capacity of the line (MW)

    // Connected lines (0-based indexing)
    std::vector<std::pair<int, int>> lines = {
        {0, 1},
        {1, 2},
        {2, 3},
        {3, 0},
        {0, 2}
    };

    // Susceptance and B matrices (initialized to zero)
    std::vector<std::vector<double>> susceptance(numBuses, std::vector<double>(numBuses, 0.0));
    std::vector<std::vector<double>> B(numBuses, std::vector<double>(numBuses, 0.0));

    // Fill susceptance matrix for existing lines
    for (const auto& line : lines) {
        int i = line.first;
        int j = line.second;
        double b_ij = -x / (r * r + x * x);
        susceptance[i][j] = b_ij;
        susceptance[j][i] = b_ij;
    }

    // Build B matrix (DC power flow)
    for (int i = 0; i < numBuses; ++i) {
        for (int j = 0; j < numBuses; ++j) {
            if (i != j) {
                B[i][j] = -susceptance[i][j];
            }
            else {
                double sum = 0.0;
                for (int k = 0; k < numBuses; ++k) {
                    if (k != i) sum += susceptance[i][k];
                }
                B[i][j] = sum;
            }
        }
    }

    // --------------------------------------------
    // C) Element Location Vectors
    // --------------------------------------------
    std::vector<int> grid_location(numBuses, 0);
    std::vector<int> battery_location(numBuses, 0);
    std::vector<int> res_location(numBuses, 0);
    std::vector<std::vector<int>> dg_locations(numDGs, std::vector<int>(numBuses, 0));
    std::vector<std::vector<int>> load_locations(numLoads, std::vector<int>(numBuses, 0));

    // Set locations
    grid_location[3] = 1;         // Grid at Bus 4
    battery_location[0] = 1;      // Battery at Bus 1
    res_location[0] = 1;          // RES at Bus 1
    dg_locations[0][1] = 1;       // DG1 at Bus 2
    dg_locations[1][2] = 1;       // DG2 at Bus 3
    load_locations[0][1] = 1;     // Load1 at Bus 2
    load_locations[1][2] = 1;     // Load2 at Bus 3

    // --------------------------------------------
    // D) Cost Parameters
    // --------------------------------------------

    // Grid Purchase Cost ($/MWh) per hour
    std::vector<double> c_buy = {
        90, 90, 90, 90, 90, 90,
        110, 110, 110, 110, 110, 125,
        125, 125, 125, 125, 125, 125,
        80, 80, 80, 80, 80, 80
    };

    // --------------------------------------------
    // E) Demand and Generation Profiles
    // --------------------------------------------

    // Load Demand (MW) per load per hour
    std::vector<std::vector<double>> p_load(numLoads, std::vector<double>{
        169, 175, 179, 171, 181, 172,
            270, 264, 273, 281, 193, 158,
            161, 162, 250, 260, 267, 271,
            284, 167, 128, 134, 144, 150
    });

    // Renewable Generation Forecast (MW) per hour
    std::vector<double> p_res = {
        16, 17, 17, 100, 181, 172,
        270, 264, 273, 281, 193, 158,
        161, 162, 250, 260, 267, 271,
        284, 167, 128, 134, 144, 150
    };

    // --------------------------------------------
    // F) Storage and Generator Parameters
    // --------------------------------------------

    // Battery Energy Storage System (BESS)
    const double p_bess_max = 200.0;   // Max charge/discharge power (MW)
    const double eta = 0.95;           // Round-trip efficiency
    const double soc_min = 0.10;       // Minimum SoC (fraction)
    const double soc_max = 0.90;       // Maximum SoC (fraction)

    // Diesel Generator Parameters
    std::vector<DieselGenerator> dgs = {
        {80.0, 50.0, 200.0},   // DG1: cost, min, max
        {70.0, 50.0, 200.0}    // DG2: cost, min, max
    };

    // --------------------------------------------
    // G) Input Summary Display
    // --------------------------------------------
    std::cout << std::fixed << std::setprecision(3);
    std::cout << "=== Microgrid Input Data ===\n\n";

    // print system parameters
    std::cout << " System Parameters:\n";
    std::cout << "  Number of Buses   : " << numBuses << "\n";
    // LINES 
    std::cout << "  Number of Lines   : " << lines.size() << "\n";
    std::cout << "  Number of Loads   : " << numLoads << "\n";
    std::cout << "  Number of DGs     : " << numDGs << "\n";
    std::cout << "  Number of BESS    : " << numBESS << "\n";
    std::cout << "  Number of RES     : " << numRES << "\n";
    std::cout << "  Time Horizon      : " << T << " hours\n";
    std::cout << "  Big M             : " << M << "\n";
    std::cout << "  Price Scale       : " << price_scale << "\n";
    std::cout << "  Consider OPF      : " << (consider_opf ? "Yes" : "No") << "\n\n";

    // Bus parameters
    std::cout << " Bus Parameters:\n";
    std::cout << "  Theta Limits      : [" << theta_min << ", " << theta_max << "]\n";

    //  Line and Network Topology
    std::cout << " Line Parameters:\n";
    std::cout << "  Reactance (x)     : " << x << " ohm\n";
    std::cout << "  Resistance (r)    : " << r << " ohm\n";
    std::cout << "  Line Capacity     : " << line_capacity << " MW\n";
    std::cout << "  Connected Lines   :\n";
    for (const auto& line : lines) {
        std::cout << "    Bus " << line.first + 1 << " <--> Bus " << line.second + 1 << "\n";
    }
    std::cout << "\n";

    // Susceptance and B matrices
    std::cout << " Susceptance Matrix (p.u.):\n";
    for (const auto& row : susceptance) {
        for (double val : row) std::cout << std::setw(10) << val;
        std::cout << "\n";
    }
    std::cout << "\n";

    std::cout << "  B Matrix (DC Power Flow):\n";
    for (const auto& row : B) {
        for (double val : row) std::cout << std::setw(10) << val;
        std::cout << "\n";
    }
    std::cout << "\n";

    // Grid Purchase Cost
    std::cout << std::fixed << std::setprecision(1);
    std::cout << " Grid Purchase Cost ($/MWh):\n  Hour :";
    for (int t = 0; t < T; ++t) std::cout << std::setw(6) << t + 1;
    std::cout << "\n  Cost :";
    for (double v : c_buy) std::cout << std::setw(6) << v;
    std::cout << "\n\n";

    // Load Demand
    for (int i = 0; i < numLoads; ++i) {
        std::cout << "Load " << (i + 1) << " Demand(MW) :\n  Hour : ";
        for (int t = 0; t < T; ++t) std::cout << std::setw(6) << t + 1;
        std::cout << "\n  Demand:";
        for (double v : p_load[i]) std::cout << std::setw(6) << v;
        std::cout << "\n\n";
    }

    // Renewable Generation
    std::cout << " Renewable Generation Forecast (MW):\n  Hour :";
    for (int t = 0; t < T; ++t) std::cout << std::setw(6) << t + 1;
    std::cout << "\n  Gen   :";
    for (double v : p_res) std::cout << std::setw(6) << v;
    std::cout << "\n\n";

    // BESS Parameters
    std::cout << " BESS Parameters:\n";
    std::cout << "  Max Power (MW)   : " << p_bess_max << "\n";
    std::cout << "  Efficiency       : " << eta * 100 << " %\n";
    std::cout << "  SoC Range        : [" << soc_min * 100 << "%, " << soc_max * 100 << "%]\n\n";

    // DG Parameters
    std::cout << " Diesel Generator Parameters:\n";
    for (size_t i = 0; i < dgs.size(); ++i) {
        std::cout << "  DG" << i + 1 << " - Cost: $" << dgs[i].cost_per_mwh << "/MWh"
            << ", Min: " << dgs[i].p_min << " MW"
            << ", Max: " << dgs[i].p_max << " MW\n";
    }
    std::cout << "\n";

    // Element Locations
    std::cout << " Element Locations Across Buses:\n";

    // Grid
    std::cout << "  Grid Location      : ";
    for (int v : grid_location) std::cout << v << " ";
    std::cout << "(Bus " << std::distance(grid_location.begin(), std::find(grid_location.begin(), grid_location.end(), 1)) + 1 << ")\n";

    // Battery
    std::cout << "  Battery Location   : ";
    for (int v : battery_location) std::cout << v << " ";
    std::cout << "(Bus " << std::distance(battery_location.begin(), std::find(battery_location.begin(), battery_location.end(), 1)) + 1 << ")\n";

    // RES
    std::cout << "  RES Location       : ";
    for (int v : res_location) std::cout << v << " ";
    std::cout << "(Bus " << std::distance(res_location.begin(), std::find(res_location.begin(), res_location.end(), 1)) + 1 << ")\n";

    // DGs
    for (int dg = 0; dg < numDGs; ++dg) {
        std::cout << "  DG" << dg + 1 << " Location     : ";
        for (int v : dg_locations[dg]) std::cout << v << " ";
        std::cout << "(Bus " << std::distance(dg_locations[dg].begin(), std::find(dg_locations[dg].begin(), dg_locations[dg].end(), 1)) + 1 << ")\n";
    }

    // Loads
    for (int ld = 0; ld < numLoads; ++ld) {
        std::cout << "  Load" << ld + 1 << " Location    : ";
        for (int v : load_locations[ld]) std::cout << v << " ";
        std::cout << "(Bus " << std::distance(load_locations[ld].begin(), std::find(load_locations[ld].begin(), load_locations[ld].end(), 1)) + 1 << ")\n";
    }


    std::cout << "\n=============================\n\n";

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

    NumVar2D theta(env, numBuses);
    for (int i = 0; i < numBuses; ++i) {
		theta[i] = IloNumVarArray(env, T, -IloInfinity, IloInfinity, ILOFLOAT);
	}

#pragma endregion

#pragma region Objective Function
    IloExpr objective(env); //Defining the expression for the objective function
    for (int t = 0; t < T; t++)
    {
        objective += c_buy[t] * p_buy[t] - price_scale * c_buy[t] * p_sell[t] ; //Grid cost
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

        if (consider_opf) {

                  // Energy balance with DC power flow
                  for (int i = 0; i < numBuses; i++)
                  {
                      IloExpr flow(env);
                      for (int j = 0; j < numBuses; j++)
                      {
                  	    flow += B[i][j] * (theta[i][t] - theta[j][t]);
                  		
                  	}
                      model.add((p_buy[t] - p_sell[t]) * grid_location[i] + (p_bess_dch[t] - p_bess_chg[t]) * battery_location[i] + p_res[t] * res_location[i] + p_dg[0][t] * dg_locations[0][i] + p_dg[1][t] * dg_locations[1][i] - p_load[0][t] *load_locations[0][i] - p_load[1][t] * load_locations[1][i] - flow == 0);

                  }

                  // Line limits
                  for (int i = 0; i < numBuses; ++i) {
                      for (int j = 0; j < numBuses; ++j) {
                          model.add(B[i][j] * (theta[i][t] - theta[j][t]) <= line_capacity);
                          model.add(B[i][j] * (theta[i][t] - theta[j][t]) >= -line_capacity);
                      }
                  }

                  // Theta limits
                  for (int i = 0; i < numBuses; ++i) {
                  	model.add(theta[i][t] >= theta_min);
                  	model.add(theta[i][t] <= theta_max);
                  }
		}
        else {
            //// Energy balance constraint without DC power flow
            model.add(p_buy[t] + p_res[t] + p_dg[0][t] + p_dg[1][t] + p_bess_dch[t] == p_bess_chg[t] + p_load[0][t] + p_load[1][t] + p_sell[t]);
        }

 
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
        resultFile << ",Load (MW),Renewable_Gen(MW),BESS_Charge(MW),BESS_Discharge(MW),SoC";
        resultFile << "\n";

        // Write data rows
        for (int t = 0; t < T; ++t) {
            resultFile << t << "," << cplex.getValue(p_buy[t]) << "," << -1 * cplex.getValue(p_sell[t]);

            for (int i = 0; i < numDGs; ++i) {
                resultFile << "," << cplex.getValue(p_dg[i][t]);
            }
            // add load, renewable generation, and BESS data
            resultFile << "," << p_load[0][t] + p_load[1][t] << "," << p_res[t] << "," << -1* cplex.getValue(p_bess_chg[t]) << "," <<  cplex.getValue(p_bess_dch[t]) << "," << cplex.getValue(soc[t]);

            resultFile << "\n";
        }

        resultFile.close();
        std::cout << "Results saved to results.csv\n";
    }
    else {
        std::cerr << "Unable to open results.csv file.\n";
    }

    if (consider_opf) {
        // -- write powerflow.csv ---
        std::ofstream pfFile("powerflow.csv");
        pfFile << "Hour";
        for (int i = 0; i < numBuses; ++i)
            for (int j = 0; j < numBuses; ++j)
                pfFile << ",P" << i + 1 << j + 1;
        pfFile << "\n";

        for (int t = 0; t < T; ++t) {
            pfFile << t;
            for (int i = 0; i < numBuses; ++i) {
                for (int j = 0; j < numBuses; ++j) {
                    double flow_ij = B[i][j] * (
                        cplex.getValue(theta[i][t])
                        - cplex.getValue(theta[j][t])
                        );
                    pfFile << "," << flow_ij;
                }
            }
            pfFile << "\n";
        }
        pfFile.close();
        std::cout << "Power-flow saved to powerflow.csv\n";

        // --- write theta.csv ---
        std::ofstream thFile("theta.csv");
        thFile << "Hour";
        for (int i = 0; i < numBuses; ++i)
            thFile << ",Theta" << (i + 1);
        thFile << "\n";

        for (int t = 0; t < T; ++t) {
            thFile << t;
            for (int i = 0; i < numBuses; ++i) {
                thFile << "," << cplex.getValue(theta[i][t]);
            }
            thFile << "\n";
        }
        thFile.close();
        std::cout << "Angles saved to theta.csv\n";
	}



#pragma endregion


    return 0;
}