// 
// File:   csg_nemat.cc
// Author: ruehle
//
// Created on March 6, 2008, 4:35 PM
//

#include <math.h>
#include <iostream>
#include <fstream>
#include <stdio.h>
#include <sstream>
#include <boost/numeric/ublas/vector.hpp>
#include <boost/numeric/ublas/matrix_sparse.hpp>
#include <boost/numeric/ublas/matrix.hpp>
#include <gsl/gsl_linalg.h>
#include <tools/cubicspline.h>
#include <cgengine.h>
#include <nblist.h>
#include <beadlist.h>
#include "csg_fmatch.h"

void CGForceMatching::BeginCG(Topology *top, Topology *top_atom)
{
    // set counters to zero value:
    _nblocks = 0;
    _line_cntr = _col_cntr = 0;

    // Number of CG beads in topology
    _nbeads = top->BeadCount();
    // Set frame counter to zero
    _frame_counter = 0;

    // read _nframes from input file
    _nframes = _options.get("cg.fmatch.frames_per_block").as<int>();
    // read _constr_least_sq from input file
    _constr_least_sq = _options.get("cg.fmatch.constrainedLS").as<bool>();
        
    // initializing bonded interactions
    for (list<Property*>::iterator iter = _bonded.begin();
            iter != _bonded.end(); ++iter) {
        SplineInfo *i = new SplineInfo(_splines.size(), true, _col_cntr, *iter);
        //adjust initial matrix dimensions:
        _line_cntr += i->n + 1;
        _col_cntr += 2 * (i->n + 1);

        // add spline to container
        _splines.push_back(i);
    }

    // initializing non-bonded interactions
    for (list<Property*>::iterator iter = _nonbonded.begin();
            iter != _nonbonded.end(); ++iter) {
        SplineInfo *i = new SplineInfo(_splines.size(), false, _col_cntr, *iter);
        //adjust initial matrix dimensions:
        _line_cntr += i->n + 1;
        _col_cntr += 2 * (i->n + 1);

        // add spline to container
        _splines.push_back(i);
    }

    cout << "\nYou are using VOTCA!\n";
    cout << "\nhey, somebody wants to forcematch!\n";


    // now initialize _A, _b, _x and probably _B_constr
    // depending on least-squares algorithm used
    if (_constr_least_sq) { // Constrained Least Squares
        
        cout << "\nUsing constrained Least Squares!\n " << endl;

        // assign _least_sq_offset
        _least_sq_offset = 0;

        // resize and clear _B_constr
        _B_constr.resize(_line_cntr, _col_cntr, false);
        _B_constr.clear();

        // resize matrix _A
        _A.resize(3 * _nbeads *_nframes, _col_cntr, false);
        // resize vector _b
        _b.resize(3 * _nbeads *_nframes, false); 

        // in case of constrained least squares smoothing conditions
        // are assigned to matrix _B_constr
        FmatchAssignSmoothCondsToMatrix(_B_constr);
    } else { // Simple Least Squares

        cout << "\nUsing simple Least Squares! " << endl;
        // assign _least_sq_offset
        _least_sq_offset = _line_cntr;

        // resize matrix _A
        _A.resize(_line_cntr + 3 * _nbeads *_nframes, _col_cntr, false);
        // resize vector _b
        _b.resize(_line_cntr + 3 * _nbeads *_nframes, false); 

        // in case of simple least squares smoothing conditions
        // are assigned to matrix _A
        FmatchAssignSmoothCondsToMatrix(_A);
        // clear _b (only necessary in simple least squares)
        _b.clear();
    }
    // resize and clear _x
    _x.resize(_col_cntr);
    _x.clear();
}

CGForceMatching::SplineInfo::SplineInfo(int index, bool bonded_, int matr_pos_, Property *options) 
{
    // initialize standard data
    splineIndex = index;
    _options = options;
    splineName = options->get("name").value();
    bonded = bonded_;

    // get non-bonded information
    if (!bonded) {
        type1 = options->get("type1").value();
        type2 = options->get("type2").value(); 
    }

    // initialize the grid
    double grid_min = options->get("fmatch.min").as<double>();
    double grid_max = options->get("fmatch.max").as<double>();
    double grid_step = options->get("fmatch.step").as<double>();

    n = Spline.GenerateGrid(grid_min, grid_max, grid_step) - 1;

    cout << "Number of spline functions for the interaction " << splineName << ":" << n << endl;

    matr_pos = matr_pos_;

    // initialize grid for block averaging
    res_output_coeff = options->get("fmatch.res_output_coeff").as<int>();
    result.resize(res_output_coeff * (n + 1), false);
    result.clear();
    error.resize(res_output_coeff * (n + 1), false);
    error.clear();
    resSum.resize(res_output_coeff * (n + 1), false);
    resSum.clear();
    resSum2.resize(res_output_coeff * (n + 1), false);
    resSum2.clear();
    block_res.resize(2 * (n + 1), false);
    del_x_out = (Spline.getGridPoint(n) - Spline.getGridPoint(0)) /
            (res_output_coeff * (n + 1));
}

void CGForceMatching::EndCG() 
{
    // sanity check
    if (_nblocks == 0) {
        cout << "\nERROR in csg_fmatch::EndCG - No blocks have been processed so far" << endl;
        cout << "It might be that you are using trajectory, which is smaller than needed for one block" << endl;
        cout << "Check your input!" << endl;
        exit(-1);
    }

    string file_extension = ".force";
    const int filename_length = 40;
    char file_name[filename_length];

    ofstream out_file;

    SplineContainer::iterator is;

    for (is = _splines.begin(); is != _splines.end(); ++is) {
        int &mp = (*is)->matr_pos;
        int &nsf = (*is)->n;

        // sanity check
        if (strlen(((*is)->splineName).c_str()) + strlen(file_extension.c_str()) > filename_length ) {
            cout << "\nERROR in csg_fmatch::EndCG - Name of the spline " << ((*is)->splineName).c_str() << " is too long" << endl;
            cout << "Don't wanna write output file with this ugly name" << endl;
            cout << "Modify your input" << endl;
            exit(-1);
        }

        file_name[0] = '\0';
        strcpy(file_name, ((*is)->splineName).c_str());
        strcat(file_name, file_extension.c_str());
        out_file.open(file_name);

        // print output file names on stdout
        cout << "Writing file: " << file_name << endl;

        out_file << "# interaction No. " << (*is)->splineIndex << endl;

        for (int i = 0; i < (*is)->res_output_coeff * (nsf + 1); i++) {
            (*is)->result[i] = (*is)->resSum[i] / _nblocks;
            (*is)->error[i] = sqrt((*is)->resSum2[i] / _nblocks - (*is)->result[i] * (*is)->result[i]);
        }

        (*is)->Spline.setSplineData((*is)->result);

        // Shitty implementation, think of adding functionality to CubicSpline
        double out_x = (*is)->Spline.getGridPoint(0);
        for (int i = 0; i < (*is)->res_output_coeff * (nsf + 1); i++) {
            out_file << out_x << " " <<
                    (-1.0) * (*is)->result[i] << " i " << (*is)->error[i] << endl;
            out_x += (*is)->del_x_out;
        }

        out_file.close();
    }
}

void CGForceMatching::EvalConfiguration(Topology *conf, Topology *conf_atom) 
{
    SplineContainer::iterator spiter;

    for (spiter = _splines.begin(); spiter != _splines.end(); ++spiter) {
        SplineInfo *sinfo = *spiter;
        if (sinfo->bonded) // bonded interaction
            EvalBonded(conf, sinfo);
        else // non-bonded interaction
            EvalNonbonded(conf, sinfo);
    }
    
    // loop for the forces vector: 
    // hack, chage the Has functions..
    if (conf->getBead(0)->HasF()) {
        vec Force(0., 0., 0.);
        for (int iatom = 0; iatom < _nbeads; ++iatom) {
            Force = conf->getBead(iatom)->getF();
            _b(_least_sq_offset + 3 * _nbeads * _frame_counter + iatom) = Force.x();
            _b(_least_sq_offset + 3 * _nbeads * _frame_counter + _nbeads + iatom) = Force.y();
            _b(_least_sq_offset + 3 * _nbeads * _frame_counter + 2 * _nbeads + iatom) = Force.z();
        }
    } else {
        cout << "ERROR: No forces in configuration!\n";
    }
    _frame_counter += 1; // update the frame counter

    if (_frame_counter % _nframes == 0) {
        _nblocks++;
        FmatchAccumulateData();
        cout << "Block No" << _nblocks << " done!" << endl;
        _frame_counter = 0;
        if (_constr_least_sq) { //Constrained Least Squares
            _A.clear();
            _b.clear();
            FmatchAssignSmoothCondsToMatrix(_B_constr);
        } else { // Simple Least Squares
            FmatchAssignSmoothCondsToMatrix(_A);
            _b.clear();
        }
    }
}

void CGForceMatching::FmatchAccumulateData() 
{
    _x.clear();
    if (_constr_least_sq) { // Constrained Least Squares
        // Solving linear equations system
        ub::matrix<double> Q;
        Q.resize(_col_cntr, _col_cntr, false);
        Q.clear();

        ub::matrix<double> A2;
        A2.resize(_A.size1(), _col_cntr / 2, false);
        A2.clear();

        ub::matrix<double> Q_k;
        Q_k.resize(_col_cntr, _col_cntr, false);
        Q_k.clear();

        ub::identity_matrix<double> I(_col_cntr);

        ub::vector<double> v;
        v.resize(_col_cntr, false);
        v.clear();

        // To proceed we need to factorize B^T = Q*R. We need matrix Q for further
        // calculations
        // B_constr_Tr - transpose of _B_constr
        ub::matrix<double> B_constr_Tr;
        B_constr_Tr.resize(_col_cntr, _line_cntr, false);
        B_constr_Tr.clear();   
        
        B_constr_Tr = trans(_B_constr);

        double* pointer_Bcnstr = & B_constr_Tr(0, 0);

        gsl_matrix_view B_t
                = gsl_matrix_view_array(pointer_Bcnstr, _col_cntr, _line_cntr);

        gsl_vector *tau = gsl_vector_alloc(_line_cntr);

        gsl_linalg_QR_decomp(&B_t.matrix, tau);

        // Extraction of Q matrix from tau and B_t, where it is stored in a tricky way.
        Q = I;

        for (int k = _line_cntr; k > 0; k--) {

            for (int icout = 0; icout < k - 1; icout++) {
                v(icout) = 0;
            }
            v(k - 1) = 1.0;

            for (int icout = k; icout < v.size(); icout++) {
                v(icout) = gsl_matrix_get(&B_t.matrix, icout, k - 1);
            }
            double tmp = gsl_vector_get(tau, k - 1);
            Q_k = I - tmp * outer_prod(v, v);
            Q = prec_prod(Q, Q_k);
        }
        Q = trans(Q);

        // Calculate _A * Q and store the result in _A
        _A = prec_prod(_A, Q);

        // _A = [A1 A2], so A2 is just a block of _A
        for (int iraw = 0; iraw < _A.size1(); iraw++) {
            for (int icol = _A.size2() / 2; icol < _A.size2(); icol++) {
                A2(iraw, icol - _A.size2() / 2) = _A(iraw, icol);
            }
        }

        double* pointer_m = & A2(0, 0);
        double* pointer_b = & _b(0);

        gsl_matrix_view m
                = gsl_matrix_view_array(pointer_m, A2.size1(), A2.size2());

        gsl_vector_view b
                = gsl_vector_view_array(pointer_b, A2.size1());

        gsl_vector *x = gsl_vector_alloc(A2.size2());
        gsl_vector *tau2 = gsl_vector_alloc(A2.size2());
        gsl_vector *residual = gsl_vector_alloc(A2.size1());

        gsl_linalg_QR_decomp(&m.matrix, tau2);

        gsl_linalg_QR_lssolve(&m.matrix, tau2, &b.vector, x, residual);

        for (int i = 0; i < _x.size() / 2; i++)
            _x[i] = 0.0;
        
        for (int i = _x.size() / 2; i < _x.size(); i++)
            _x[i] = gsl_vector_get(x, i - _x.size() / 2);

        // To get the final answer this vector should be multiplied by matrix Q
        _x = prec_prod(Q, _x);

        gsl_vector_free(x);
        gsl_vector_free(tau);
        gsl_vector_free(residual);

    } else { // Simple Least Squares
        double* pointer_m = & _A(0, 0);
        double* pointer_b = & _b(0);

        gsl_matrix_view m
                = gsl_matrix_view_array(pointer_m, _A.size1(), _A.size2());

        gsl_vector_view b
                = gsl_vector_view_array(pointer_b, _b.size());

        gsl_vector *x = gsl_vector_alloc(_A.size2());
        gsl_vector *tau = gsl_vector_alloc(_A.size2());
        gsl_vector *residual = gsl_vector_alloc(_b.size());

        gsl_linalg_QR_decomp(&m.matrix, tau);
        gsl_linalg_QR_lssolve(&m.matrix, tau, &b.vector, x, residual);

        for (int i = 0; i < _x.size(); i++)
            _x(i) = gsl_vector_get(x, i);        

        // calculate FM residual - quality of FM
        // FM residual is initially calculated in (kJ/(mol*nm))^2
        double fm_resid = 0;

        for (int i = 0; i < _b.size(); i++)
            fm_resid += gsl_vector_get(residual, i) * gsl_vector_get(residual, i);

        // strange number is units conversion -> now (kcal/(mol*angstrom))^2
        fm_resid /= 3 * _nbeads * _frame_counter * 1750.5856;

        cout << endl;
        cout << "#### Force matching residual ####" << endl;
        cout << "     Chi_2 = " << fm_resid << endl;
        cout << "#################################" << endl;
        cout << endl;

        gsl_vector_free(x);
        gsl_vector_free(tau);
        gsl_vector_free(residual);
    }

    SplineContainer::iterator is;
    for (is = _splines.begin(); is != _splines.end(); ++is) {
        int &mp = (*is)->matr_pos;
        int &nsf = (*is)->n;

        for (int i = 0; i < 2 * (nsf + 1); i++) {
            (*is)->block_res[i] = _x[ i + mp ];
            //                (*is)->resSum[i] += _x[ i + mp ];
            //                (*is)->resSum2[i] += _x[ i + mp ] * _x[ i + mp ];
        }
        (*is)->Spline.setSplineData((*is)->block_res);

        double out_x = (*is)->Spline.getGridPoint(0);
        for (int i = 0; i < (*is)->res_output_coeff * (nsf + 1); i++) {
            (*is)->resSum[i] += (*is)->Spline.Calculate(out_x);
            if (i == 23) cout << (*is)->Spline.Calculate(out_x) << " " << endl;
            (*is)->resSum2[i] += (*is)->Spline.Calculate(out_x) * (*is)->Spline.Calculate(out_x);
            out_x += (*is)->del_x_out;
        }
    }
}

void CGForceMatching::FmatchAssignSmoothCondsToMatrix(ub::matrix<double> &Matrix)
{
// This function assigns Spline smoothing conditions to the Matrix.
// For the simple least squares the function is used for matrix _A
// For constrained least squares - for matrix _B_constr
    int line_tmp, col_tmp;
    line_tmp = 0;
    col_tmp = 0;

    Matrix.clear();


    SplineContainer::iterator is;
    for (is = _splines.begin(); is != _splines.end(); ++is) {
        int sfnum = (*is)->n;
        (*is)->Spline.AddBCToFitMatrix(Matrix, line_tmp, col_tmp);
        // update counters
        line_tmp += sfnum + 1;
        col_tmp += 2 * (sfnum + 1);
    }
}

void CGForceMatching::LoadOptions(const string &file) 
{
    load_property_from_xml(_options, file);
    _bonded = _options.Select("cg.bonded");
    _nonbonded = _options.Select("cg.non-bonded");
}

void CGForceMatching::EvalBonded(Topology *conf, SplineInfo *sinfo) 
{
    std::list<Interaction *> interList;
    std::list<Interaction *>::iterator interListIter;

    interList = conf->InteractionsInGroup(sinfo->splineName);

    for (interListIter = interList.begin(); interListIter != interList.end(); ++interListIter) {

        int beads_in_int = (*interListIter)->BeadCount(); // 2 for bonds, 3 for angles, 4 for dihedrals

        CubicSpline &SP = sinfo->Spline;

        int &mpos = sinfo->matr_pos;
        int &nsp = sinfo->n;

        double var = (*interListIter)->EvaluateVar(*conf); // value of bond, angle, or dihedral
        int i = SP.getInterval(var); // corresponding spline interval

        for (int loop = 0; loop < beads_in_int; loop++) {
            int ii = (*interListIter)->getBeadId(loop);
            vec gradient = (*interListIter)->Grad(*conf, loop);

            SP.AddToFitMatrix(_A, var,
                    _least_sq_offset + 3 * _nbeads * _frame_counter + ii, mpos, -gradient.x());
            SP.AddToFitMatrix(_A, var,
                    _least_sq_offset + 3 * _nbeads * _frame_counter + _nbeads + ii, mpos, -gradient.y());
            SP.AddToFitMatrix(_A, var,
                    _least_sq_offset + 3 * _nbeads * _frame_counter + 2 * _nbeads + ii, mpos, -gradient.z());
        }
    }
}

void CGForceMatching::EvalNonbonded(Topology *conf, SplineInfo *sinfo) 
{
    // generate the neighbour list
    NBList NBL;
    NBL.setCutoff(sinfo->_options->get("fmatch.max").as<double>()); // implement different cutoffs for different interactions!

    // generate the bead lists
    BeadList beads1, beads2;
    beads1.Generate(*conf, sinfo->type1);
    beads2.Generate(*conf, sinfo->type2);

    // is it same types or different types?
    if (sinfo->type1 == sinfo->type2)
        NBL.Generate(beads1, true);
    else
        NBL.Generate(beads1, beads2, true);

    NBList::iterator pair_iter;
    // iterate over all pairs
    for (pair_iter = NBL.begin(); pair_iter != NBL.end(); ++pair_iter) {
        int iatom = (*pair_iter)->first->getId();
        int jatom = (*pair_iter)->second->getId();
        double var = (*pair_iter)->dist();
        vec gradient = (*pair_iter)->r();
        gradient.normalize();

        CubicSpline &SP = sinfo->Spline;

        int &mpos = sinfo->matr_pos;
        int &nsp = sinfo->n;
        int i = SP.getInterval(var);

        // add iatom
        SP.AddToFitMatrix(_A, var,
                _least_sq_offset + 3 * _nbeads * _frame_counter + iatom, mpos, gradient.x());
        SP.AddToFitMatrix(_A, var,
                _least_sq_offset + 3 * _nbeads * _frame_counter + _nbeads + iatom, mpos, gradient.y());
        SP.AddToFitMatrix(_A, var,
                _least_sq_offset + 3 * _nbeads * _frame_counter + 2 * _nbeads + iatom, mpos, gradient.z());

        // add jatom 
        SP.AddToFitMatrix(_A, var,
                _least_sq_offset + 3 * _nbeads * _frame_counter + jatom, mpos, -gradient.x());
        SP.AddToFitMatrix(_A, var,
                _least_sq_offset + 3 * _nbeads * _frame_counter + _nbeads + jatom, mpos, -gradient.y());
        SP.AddToFitMatrix(_A, var,
                _least_sq_offset + 3 * _nbeads * _frame_counter + 2 * _nbeads + jatom, mpos, -gradient.z());
    }
}
