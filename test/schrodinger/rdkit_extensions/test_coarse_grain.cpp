/* -------------------------------------------------------------------------
 * Tests class schrodinger::rdkit_extensions:: coarse_grain
 *
 * Copyright Schrodinger LLC, All Rights Reserved.
 --------------------------------------------------------------------------- */

#define BOOST_TEST_MODULE rdkit_extensions_coarse_grain

#include <boost/test/unit_test.hpp>

#include <rdkit/GraphMol/FileParsers/FileParsers.h>
#include <rdkit/GraphMol/GraphMol.h>
#include <rdkit/GraphMol/SmilesParse/SmilesParse.h>
#include <rdkit/GraphMol/SmilesParse/SmilesWrite.h>
#include <rdkit/RDGeneral/RDLog.h>

#include "schrodinger/rdkit_extensions/capture_rdkit_log.h"
#include "schrodinger/rdkit_extensions/coarse_grain.h"
#include "schrodinger/rdkit_extensions/cg_conversions.h"
#include "schrodinger/rdkit_extensions/convert.h"
#include "schrodinger/rdkit_extensions/helm.h"
#include "test_common.h"

using namespace schrodinger::rdkit_extensions;

BOOST_AUTO_TEST_CASE(TestBasicCoarseGrainMol)
{
    ::RDKit::RWMol cg_mol;
    auto monomer_idx1 = add_monomer(cg_mol, "A", 1, "PEPTIDE1");
    auto monomer_idx2 = add_monomer(cg_mol, "G");
    auto monomer_idx3 = add_monomer(cg_mol, "C");
    add_connection(cg_mol, monomer_idx1, monomer_idx2);
    add_connection(cg_mol, monomer_idx2, monomer_idx3);
    assign_chains(cg_mol);

    BOOST_CHECK_EQUAL(to_string(cg_mol, Format::HELM),
                      "PEPTIDE1{A.G.C}$$$$V2.0");
    BOOST_CHECK_EQUAL(to_string(cg_mol, Format::FASTA), ">\nAGC\n");

    // making it a cyclic peptide
    add_connection(cg_mol, 2, 0);
    assign_chains(cg_mol);
    BOOST_CHECK_EQUAL(to_string(cg_mol, Format::HELM),
                      "PEPTIDE1{A.G.C}$PEPTIDE1,PEPTIDE1,3:R2-1:R1$$$V2.0");
    // This is a cyclic peptide, should FASTA throw?
    // BOOST_CHECK_THROW(to_string(cg_mol, Format::FASTA),
    // std::invalid_argument);
}

BOOST_AUTO_TEST_CASE(TestBranchesCoarseGrain)
{
    ::RDKit::RWMol cg_mol;
    auto monomer_idx1 = add_monomer(cg_mol, "A", 1, "PEPTIDE1");
    auto monomer_idx2 = add_monomer(cg_mol, "C");
    auto monomer_branch = add_monomer(cg_mol, "A");
    auto monomer_idx3 = add_monomer(cg_mol, "C");
    auto monomer_idx4 = add_monomer(cg_mol, "T");

    add_connection(cg_mol, monomer_idx1, monomer_idx2);
    add_connection(cg_mol, monomer_idx2, monomer_idx3);
    add_connection(cg_mol, monomer_idx3, monomer_idx4);
    add_connection(cg_mol, monomer_idx2, monomer_branch,
                   ConnectionType::SIDECHAIN);
    assign_chains(cg_mol);
    BOOST_CHECK_EQUAL(to_string(cg_mol, Format::HELM),
                      "PEPTIDE1{A.C(A)C.T}$$$$V2.0");

    auto monomer_branch2 = add_monomer(cg_mol, "A");
    add_connection(cg_mol, monomer_idx3, monomer_branch2,
                   ConnectionType::SIDECHAIN);
    assign_chains(cg_mol);
    BOOST_CHECK_EQUAL(to_string(cg_mol, Format::HELM),
                      "PEPTIDE1{A.C(A)C(A)T}$$$$V2.0");
    // TODO (SHARED-10771): Fix HELM writer, there should be a `.` bewteen
    // consecutive branches
}

BOOST_AUTO_TEST_CASE(TestMultipleChainsCoarseGrainMol)
{
    ::RDKit::RWMol cg_mol;
    auto monomer_idx1 = add_monomer(cg_mol, "A", 1, "PEPTIDE1");
    auto monomer_idx2 = add_monomer(cg_mol, "G");
    auto monomer_idx3 = add_monomer(cg_mol, "C");
    add_connection(cg_mol, monomer_idx1, monomer_idx2);
    add_connection(cg_mol, monomer_idx2, monomer_idx3);

    auto monomer_idx4 = add_monomer(cg_mol, "T", 1, "PEPTIDE2");
    auto monomer_idx5 = add_monomer(cg_mol, "C");
    auto monomer_idx6 = add_monomer(cg_mol, "A");
    add_connection(cg_mol, monomer_idx4, monomer_idx5);
    add_connection(cg_mol, monomer_idx5, monomer_idx6);

    add_connection(cg_mol, monomer_idx3, monomer_idx4,
                   ConnectionType::SIDECHAIN);
    assign_chains(cg_mol);

    BOOST_CHECK(
        cg_mol.getAtomWithIdx(monomer_idx4)->getProp<bool>(BRANCH_MONOMER));
    BOOST_CHECK(
        !cg_mol.getAtomWithIdx(monomer_idx3)->getProp<bool>(BRANCH_MONOMER));
    BOOST_CHECK_EQUAL(
        to_string(cg_mol, Format::HELM),
        "PEPTIDE1{A.G.C}|PEPTIDE2{T.C.A}$PEPTIDE1,PEPTIDE2,3:R3-1:R1$$$V2.0");
}

BOOST_AUTO_TEST_CASE(TestAtomisticSmilesToCGString)
{
    {
        std::unique_ptr<::RDKit::RWMol> mol(::RDKit::SmilesToMol(
            "CC[C@H](C)[C@H](NC(=O)[C@H](CC(C)C)NC(=O)[C@H](CCC(=O)O)NC(=O)[C@"
            "H](CCC(=O)O)NC(=O)[C@@H](NC(=O)[C@H](CC(C)C)NC(=O)[C@H](CO)NC(=O)["
            "C@@H](NC(=O)[C@H](Cc1ccc(O)cc1)NC(C)=O)[C@@H](C)O)[C@@H](C)CC)C(="
            "O)N[C@@H](CCCCN)C(=O)N[C@@H](CCCCN)C(=O)N[C@H](C(=O)N[C@@H](CCC(="
            "O)O)C(=O)N[C@@H](CCC(=O)O)C(=O)N[C@@H](CCC(N)=O)C(=O)N[C@@H](CCC("
            "N)=O)C(=O)N[C@@H](CCCCN)C(=O)N[C@@H](CCCCN)C(=O)N[C@@H](CC(N)=O)C("
            "=O)N[C@@H](CCC(=O)O)C(=O)N[C@@H](CCC(=O)O)C(=O)N[C@@H](CCC(=O)O)C("
            "=O)N[C@@H](CC(C)C)C(=O)N[C@@H](CCCCN)C(=O)N[C@@H](CCCCN)C(=O)N[C@@"
            "H](CC(C)C)C(=O)N[C@@H](CCC(=O)O)C(=O)N[C@@H](CCC(=O)O)C(=O)N[C@@H]"
            "(Cc1c[nH]c2ccccc12)C(=O)N[C@@H](C)C(=O)N[C@@H](CCCCN)C(=O)N[C@@H]("
            "CCCCN)C(=O)N[C@@H](Cc1c[nH]c2ccccc12)C(=O)N[C@@H](CC(N)=O)C(=O)N["
            "C@@H](Cc1c[nH]c2ccccc12)C(=O)N[C@@H](Cc1ccccc1)C(N)=O)[C@@H](C)"
            "O"));
        bool use_residue_info = false;
        auto cg_mol = atomistic_to_cg(*mol, use_residue_info);
        BOOST_CHECK_EQUAL(to_string(*cg_mol, Format::HELM),
                          "PEPTIDE1{Y.T.S.L.I.E.E.L.I.K.K.T.E.E.Q.Q.K.K.N.E.E."
                          "E.L.K.K.L.E.E.W.A.K.K.W.N.W.F}$$$$V2.0");
        BOOST_CHECK_EQUAL(to_string(*cg_mol, Format::FASTA),
                          ">\nYTSLIEELIKKTEEQQKKNEEELKKLEEWAKKWNWF\n");
    }

    {
        // cyclic peptide
        std::unique_ptr<::RDKit::RWMol> mol(::RDKit::SmilesToMol(
            "CCC(C)[C@@H]1NC(=O)C2CCCN2C(=O)[C@H](CCCCN)NC(=O)[C@H](CC(N)=O)NC("
            "=O)[C@H](CC(C)C)NC(=O)[C@H](Cc2c[nH]c3ccccc23)NC(=O)[C@H](CC(C)C)"
            "NC(=O)[C@H](Cc2ccccc2)NC(=O)[C@@H]2CCCN2C(=O)[C@H]2CCCN2C1=O"));
        bool use_residue_info = false;
        auto cg_mol = atomistic_to_cg(*mol, use_residue_info);
        BOOST_CHECK_EQUAL(to_string(*cg_mol, Format::HELM),
                          "PEPTIDE1{P.P.F.L.W.L.N.K.P.I}$PEPTIDE1,PEPTIDE1,10:"
                          "R2-1:R1$$$V2.0");
    }
}

constexpr auto PDB_1DNG = R"(
SEQRES   1 A   15  GLN ALA PRO ALA TYR GLU GLU ALA ALA GLU GLU LEU ALA
SEQRES   2 A   15  LYS SER
ATOM      1  N   GLN A   1     -11.941  -0.843  -3.601  1.00  3.05           N
ATOM      2  CA  GLN A   1     -11.467   0.517  -3.195  1.00  2.47           C
ATOM      3  C   GLN A   1     -10.086   0.425  -2.532  1.00  1.89           C
ATOM      4  O   GLN A   1      -9.611  -0.651  -2.219  1.00  2.39           O
ATOM      5  CB  GLN A   1     -12.509   1.027  -2.195  1.00  3.22           C
ATOM      6  CG  GLN A   1     -13.569   1.851  -2.933  1.00  3.81           C
ATOM      7  CD  GLN A   1     -14.788   2.047  -2.028  1.00  4.75           C
ATOM      8  OE1 GLN A   1     -14.842   2.986  -1.260  1.00  5.18           O
ATOM      9  NE2 GLN A   1     -15.774   1.194  -2.084  1.00  5.44           N
ATOM     10  H1  GLN A   1     -11.343  -1.203  -4.371  1.00  3.39           H
ATOM     11  H2  GLN A   1     -12.928  -0.785  -3.926  1.00  3.45           H
ATOM     12  H3  GLN A   1     -11.879  -1.489  -2.788  1.00  3.34           H
ATOM     13  HA  GLN A   1     -11.428   1.171  -4.052  1.00  2.68           H
ATOM     14  HB2 GLN A   1     -12.981   0.187  -1.706  1.00  3.69           H
ATOM     15  HB3 GLN A   1     -12.026   1.648  -1.455  1.00  3.50           H
ATOM     16  HG2 GLN A   1     -13.156   2.815  -3.194  1.00  3.93           H
ATOM     17  HG3 GLN A   1     -13.869   1.332  -3.830  1.00  4.00           H
ATOM     18 HE21 GLN A   1     -15.732   0.434  -2.701  1.00  5.45           H
ATOM     19 HE22 GLN A   1     -16.558   1.313  -1.507  1.00  6.15           H
ATOM     20  N   ALA A   2      -9.442   1.546  -2.317  1.00  1.59           N
ATOM     21  CA  ALA A   2      -8.091   1.532  -1.674  1.00  1.49           C
ATOM     22  C   ALA A   2      -8.035   2.564  -0.535  1.00  0.86           C
ATOM     23  O   ALA A   2      -7.651   3.699  -0.745  1.00  1.64           O
ATOM     24  CB  ALA A   2      -7.119   1.900  -2.798  1.00  2.53           C
ATOM     25  H   ALA A   2      -9.848   2.400  -2.578  1.00  2.06           H
ATOM     26  HA  ALA A   2      -7.864   0.546  -1.300  1.00  1.83           H
ATOM     27  HB1 ALA A   2      -6.356   1.140  -2.877  1.00  3.01           H
ATOM     28  HB2 ALA A   2      -6.658   2.852  -2.580  1.00  2.97           H
ATOM     29  HB3 ALA A   2      -7.656   1.967  -3.733  1.00  2.93           H
ATOM     30  N   PRO A   3      -8.440   2.133   0.638  1.00  0.84           N
ATOM     31  CA  PRO A   3      -8.453   3.027   1.821  1.00  1.61           C
ATOM     32  C   PRO A   3      -7.077   3.074   2.503  1.00  1.48           C
ATOM     33  O   PRO A   3      -6.855   2.430   3.513  1.00  2.39           O
ATOM     34  CB  PRO A   3      -9.489   2.380   2.731  1.00  2.52           C
ATOM     35  CG  PRO A   3      -9.521   0.929   2.350  1.00  2.50           C
ATOM     36  CD  PRO A   3      -8.927   0.789   0.969  1.00  1.74           C
ATOM     37  HA  PRO A   3      -8.773   4.017   1.544  1.00  2.12           H
ATOM     38  HB2 PRO A   3      -9.200   2.493   3.767  1.00  3.01           H
ATOM     39  HB3 PRO A   3     -10.454   2.823   2.561  1.00  2.97           H
ATOM     40  HG2 PRO A   3      -8.940   0.354   3.059  1.00  2.77           H
ATOM     41  HG3 PRO A   3     -10.540   0.575   2.342  1.00  3.13           H
ATOM     42  HD2 PRO A   3      -8.110   0.082   0.983  1.00  2.04           H
ATOM     43  HD3 PRO A   3      -9.682   0.483   0.262  1.00  2.04           H
ATOM     44  N   ALA A   4      -6.158   3.837   1.956  1.00  0.81           N
ATOM     45  CA  ALA A   4      -4.787   3.948   2.556  1.00  1.02           C
ATOM     46  C   ALA A   4      -4.137   2.563   2.732  1.00  1.13           C
ATOM     47  O   ALA A   4      -3.219   2.398   3.516  1.00  1.38           O
ATOM     48  CB  ALA A   4      -4.996   4.623   3.916  1.00  1.52           C
ATOM     49  H   ALA A   4      -6.372   4.345   1.144  1.00  1.02           H
ATOM     50  HA  ALA A   4      -4.163   4.572   1.936  1.00  1.01           H
ATOM     51  HB1 ALA A   4      -5.303   3.885   4.642  1.00  1.84           H
ATOM     52  HB2 ALA A   4      -5.761   5.381   3.828  1.00  2.05           H
ATOM     53  HB3 ALA A   4      -4.072   5.081   4.236  1.00  1.90           H
ATOM     54  N   TYR A   5      -4.595   1.573   2.004  1.00  1.09           N
ATOM     55  CA  TYR A   5      -3.997   0.209   2.123  1.00  1.37           C
ATOM     56  C   TYR A   5      -3.163  -0.101   0.872  1.00  1.13           C
ATOM     57  O   TYR A   5      -2.182  -0.818   0.934  1.00  1.17           O
ATOM     58  CB  TYR A   5      -5.199  -0.747   2.286  1.00  1.81           C
ATOM     59  CG  TYR A   5      -5.450  -1.544   1.021  1.00  2.03           C
ATOM     60  CD1 TYR A   5      -6.127  -0.958  -0.057  1.00  2.40           C
ATOM     61  CD2 TYR A   5      -5.009  -2.870   0.932  1.00  2.54           C
ATOM     62  CE1 TYR A   5      -6.361  -1.698  -1.222  1.00  2.75           C
ATOM     63  CE2 TYR A   5      -5.246  -3.610  -0.233  1.00  2.91           C
ATOM     64  CZ  TYR A   5      -5.921  -3.023  -1.310  1.00  2.82           C
ATOM     65  OH  TYR A   5      -6.156  -3.753  -2.458  1.00  3.30           O
ATOM     66  H   TYR A   5      -5.327   1.728   1.373  1.00  0.97           H
ATOM     67  HA  TYR A   5      -3.371   0.158   2.997  1.00  1.57           H
ATOM     68  HB2 TYR A   5      -4.999  -1.430   3.098  1.00  2.16           H
ATOM     69  HB3 TYR A   5      -6.081  -0.169   2.522  1.00  1.80           H
ATOM     70  HD1 TYR A   5      -6.465   0.065   0.011  1.00  2.79           H
ATOM     71  HD2 TYR A   5      -4.488  -3.323   1.762  1.00  2.99           H
ATOM     72  HE1 TYR A   5      -6.882  -1.245  -2.053  1.00  3.30           H
ATOM     73  HE2 TYR A   5      -4.906  -4.632  -0.301  1.00  3.55           H
ATOM     74  HH  TYR A   5      -5.323  -3.844  -2.928  1.00  3.52           H
ATOM     75  N   GLU A   6      -3.539   0.448  -0.255  1.00  1.06           N
ATOM     76  CA  GLU A   6      -2.757   0.197  -1.508  1.00  1.19           C
ATOM     77  C   GLU A   6      -1.356   0.812  -1.388  1.00  0.91           C
ATOM     78  O   GLU A   6      -0.393   0.288  -1.914  1.00  1.01           O
ATOM     79  CB  GLU A   6      -3.550   0.849  -2.651  1.00  1.51           C
ATOM     80  CG  GLU A   6      -3.916   2.297  -2.301  1.00  1.81           C
ATOM     81  CD  GLU A   6      -3.842   3.161  -3.560  1.00  2.57           C
ATOM     82  OE1 GLU A   6      -4.761   3.091  -4.360  1.00  3.13           O
ATOM     83  OE2 GLU A   6      -2.867   3.878  -3.701  1.00  3.07           O
ATOM     84  H   GLU A   6      -4.330   1.031  -0.272  1.00  1.08           H
ATOM     85  HA  GLU A   6      -2.675  -0.861  -1.680  1.00  1.43           H
ATOM     86  HB2 GLU A   6      -2.950   0.841  -3.549  1.00  1.90           H
ATOM     87  HB3 GLU A   6      -4.455   0.285  -2.824  1.00  1.92           H
ATOM     88  HG2 GLU A   6      -4.919   2.329  -1.901  1.00  2.09           H
ATOM     89  HG3 GLU A   6      -3.226   2.679  -1.567  1.00  1.99           H
ATOM     90  N   GLU A   7      -1.236   1.906  -0.679  1.00  0.74           N
ATOM     91  CA  GLU A   7       0.104   2.545  -0.501  1.00  0.87           C
ATOM     92  C   GLU A   7       0.976   1.667   0.402  1.00  0.69           C
ATOM     93  O   GLU A   7       2.169   1.536   0.196  1.00  0.84           O
ATOM     94  CB  GLU A   7      -0.167   3.909   0.152  1.00  1.14           C
ATOM     95  CG  GLU A   7      -0.846   3.722   1.520  1.00  1.41           C
ATOM     96  CD  GLU A   7      -1.459   5.044   1.995  1.00  1.96           C
ATOM     97  OE1 GLU A   7      -1.981   5.775   1.167  1.00  2.55           O
ATOM     98  OE2 GLU A   7      -1.402   5.300   3.185  1.00  2.41           O
ATOM     99  H   GLU A   7      -2.024   2.294  -0.252  1.00  0.73           H
ATOM    100  HA  GLU A   7       0.580   2.680  -1.456  1.00  1.13           H
ATOM    101  HB2 GLU A   7       0.768   4.431   0.286  1.00  1.64           H
ATOM    102  HB3 GLU A   7      -0.813   4.489  -0.490  1.00  1.44           H
ATOM    103  HG2 GLU A   7      -1.624   2.978   1.437  1.00  1.78           H
ATOM    104  HG3 GLU A   7      -0.112   3.391   2.240  1.00  1.77           H
ATOM    105  N   ALA A   8       0.375   1.051   1.389  1.00  0.61           N
ATOM    106  CA  ALA A   8       1.146   0.159   2.305  1.00  0.82           C
ATOM    107  C   ALA A   8       1.568  -1.109   1.555  1.00  0.65           C
ATOM    108  O   ALA A   8       2.571  -1.714   1.875  1.00  0.73           O
ATOM    109  CB  ALA A   8       0.187  -0.181   3.446  1.00  1.24           C
ATOM    110  H   ALA A   8      -0.588   1.166   1.516  1.00  0.62           H
ATOM    111  HA  ALA A   8       2.012   0.674   2.690  1.00  1.01           H
ATOM    112  HB1 ALA A   8       0.702  -0.782   4.181  1.00  1.62           H
ATOM    113  HB2 ALA A   8      -0.655  -0.733   3.056  1.00  1.77           H
ATOM    114  HB3 ALA A   8      -0.161   0.731   3.907  1.00  1.66           H
ATOM    115  N   ALA A   9       0.820  -1.504   0.548  1.00  0.73           N
ATOM    116  CA  ALA A   9       1.196  -2.721  -0.235  1.00  1.01           C
ATOM    117  C   ALA A   9       2.581  -2.514  -0.858  1.00  0.82           C
ATOM    118  O   ALA A   9       3.409  -3.407  -0.866  1.00  0.90           O
ATOM    119  CB  ALA A   9       0.127  -2.851  -1.323  1.00  1.42           C
ATOM    120  H   ALA A   9       0.021  -0.990   0.300  1.00  0.79           H
ATOM    121  HA  ALA A   9       1.191  -3.595   0.398  1.00  1.23           H
ATOM    122  HB1 ALA A   9       0.283  -2.090  -2.074  1.00  1.92           H
ATOM    123  HB2 ALA A   9      -0.852  -2.726  -0.883  1.00  1.86           H
ATOM    124  HB3 ALA A   9       0.195  -3.827  -1.780  1.00  1.67           H
ATOM    125  N   GLU A  10       2.842  -1.327  -1.355  1.00  0.79           N
ATOM    126  CA  GLU A  10       4.178  -1.031  -1.956  1.00  1.03           C
ATOM    127  C   GLU A  10       5.229  -0.927  -0.846  1.00  0.79           C
ATOM    128  O   GLU A  10       6.358  -1.344  -1.011  1.00  0.93           O
ATOM    129  CB  GLU A  10       4.020   0.320  -2.661  1.00  1.41           C
ATOM    130  CG  GLU A  10       3.367   0.115  -4.029  1.00  1.89           C
ATOM    131  CD  GLU A  10       2.613   1.385  -4.434  1.00  2.17           C
ATOM    132  OE1 GLU A  10       1.490   1.552  -3.988  1.00  2.70           O
ATOM    133  OE2 GLU A  10       3.173   2.168  -5.184  1.00  2.47           O
ATOM    134  H   GLU A  10       2.160  -0.623  -1.315  1.00  0.79           H
ATOM    135  HA  GLU A  10       4.452  -1.793  -2.669  1.00  1.27           H
ATOM    136  HB2 GLU A  10       3.400   0.970  -2.059  1.00  1.80           H
ATOM    137  HB3 GLU A  10       4.991   0.772  -2.793  1.00  1.80           H
ATOM    138  HG2 GLU A  10       4.130  -0.100  -4.762  1.00  2.38           H
ATOM    139  HG3 GLU A  10       2.675  -0.711  -3.977  1.00  2.30           H
ATOM    140  N   GLU A  11       4.854  -0.384   0.289  1.00  0.65           N
ATOM    141  CA  GLU A  11       5.817  -0.259   1.426  1.00  0.89           C
ATOM    142  C   GLU A  11       6.115  -1.647   2.002  1.00  0.63           C
ATOM    143  O   GLU A  11       7.214  -1.923   2.444  1.00  0.77           O
ATOM    144  CB  GLU A  11       5.099   0.618   2.459  1.00  1.28           C
ATOM    145  CG  GLU A  11       6.124   1.257   3.402  1.00  1.74           C
ATOM    146  CD  GLU A  11       6.619   2.580   2.809  1.00  2.29           C
ATOM    147  OE1 GLU A  11       6.011   3.599   3.094  1.00  2.95           O
ATOM    148  OE2 GLU A  11       7.596   2.552   2.078  1.00  2.70           O
ATOM    149  H   GLU A  11       3.932  -0.072   0.397  1.00  0.61           H
ATOM    150  HA  GLU A  11       6.727   0.220   1.100  1.00  1.17           H
ATOM    151  HB2 GLU A  11       4.547   1.394   1.948  1.00  1.67           H
ATOM    152  HB3 GLU A  11       4.416   0.009   3.033  1.00  1.91           H
ATOM    153  HG2 GLU A  11       5.660   1.442   4.360  1.00  2.18           H
ATOM    154  HG3 GLU A  11       6.960   0.587   3.532  1.00  2.25           H
ATOM    155  N   LEU A  12       5.138  -2.522   1.984  1.00  0.56           N
ATOM    156  CA  LEU A  12       5.339  -3.905   2.512  1.00  0.88           C
ATOM    157  C   LEU A  12       6.368  -4.647   1.654  1.00  0.73           C
ATOM    158  O   LEU A  12       7.303  -5.234   2.160  1.00  0.91           O
ATOM    159  CB  LEU A  12       3.967  -4.576   2.396  1.00  1.23           C
ATOM    160  CG  LEU A  12       3.097  -4.192   3.595  1.00  1.62           C
ATOM    161  CD1 LEU A  12       1.723  -4.853   3.461  1.00  2.19           C
ATOM    162  CD2 LEU A  12       3.764  -4.666   4.891  1.00  2.30           C
ATOM    163  H   LEU A  12       4.266  -2.267   1.613  1.00  0.54           H
ATOM    164  HA  LEU A  12       5.654  -3.876   3.543  1.00  1.16           H
ATOM    165  HB2 LEU A  12       3.486  -4.250   1.485  1.00  1.51           H
ATOM    166  HB3 LEU A  12       4.093  -5.644   2.371  1.00  1.79           H
ATOM    167  HG  LEU A  12       2.979  -3.119   3.619  1.00  1.94           H
ATOM    168 HD11 LEU A  12       1.822  -5.920   3.593  1.00  2.53           H
ATOM    169 HD12 LEU A  12       1.320  -4.647   2.480  1.00  2.74           H
ATOM    170 HD13 LEU A  12       1.058  -4.456   4.214  1.00  2.52           H
ATOM    171 HD21 LEU A  12       4.258  -5.610   4.718  1.00  2.74           H
ATOM    172 HD22 LEU A  12       3.014  -4.787   5.659  1.00  2.60           H
ATOM    173 HD23 LEU A  12       4.490  -3.933   5.211  1.00  2.80           H
ATOM    174  N   ALA A  13       6.200  -4.611   0.355  1.00  0.67           N
ATOM    175  CA  ALA A  13       7.170  -5.301  -0.554  1.00  0.98           C
ATOM    176  C   ALA A  13       8.493  -4.525  -0.597  1.00  0.84           C
ATOM    177  O   ALA A  13       9.553  -5.097  -0.768  1.00  1.05           O
ATOM    178  CB  ALA A  13       6.502  -5.292  -1.929  1.00  1.36           C
ATOM    179  H   ALA A  13       5.438  -4.121  -0.023  1.00  0.64           H
ATOM    180  HA  ALA A  13       7.337  -6.316  -0.229  1.00  1.22           H
ATOM    181  HB1 ALA A  13       6.356  -4.272  -2.251  1.00  1.74           H
ATOM    182  HB2 ALA A  13       5.546  -5.791  -1.867  1.00  1.58           H
ATOM    183  HB3 ALA A  13       7.132  -5.808  -2.638  1.00  1.92           H
ATOM    184  N   LYS A  14       8.429  -3.225  -0.439  1.00  0.74           N
ATOM    185  CA  LYS A  14       9.664  -2.388  -0.461  1.00  1.06           C
ATOM    186  C   LYS A  14      10.101  -2.074   0.982  1.00  1.05           C
ATOM    187  O   LYS A  14      10.254  -0.930   1.365  1.00  1.53           O
ATOM    188  CB  LYS A  14       9.241  -1.118  -1.212  1.00  1.45           C
ATOM    189  CG  LYS A  14      10.407  -0.128  -1.295  1.00  2.00           C
ATOM    190  CD  LYS A  14      10.002   1.186  -0.618  1.00  2.62           C
ATOM    191  CE  LYS A  14       8.824   1.819  -1.373  1.00  2.78           C
ATOM    192  NZ  LYS A  14       9.435   2.804  -2.313  1.00  3.58           N
ATOM    193  H   LYS A  14       7.558  -2.795  -0.302  1.00  0.64           H
ATOM    194  HA  LYS A  14      10.454  -2.891  -0.996  1.00  1.25           H
ATOM    195  HB2 LYS A  14       8.926  -1.383  -2.211  1.00  1.81           H
ATOM    196  HB3 LYS A  14       8.418  -0.655  -0.690  1.00  1.72           H
ATOM    197  HG2 LYS A  14      11.271  -0.543  -0.795  1.00  2.43           H
ATOM    198  HG3 LYS A  14      10.645   0.062  -2.331  1.00  2.42           H
ATOM    199  HD2 LYS A  14       9.708   0.984   0.403  1.00  3.07           H
ATOM    200  HD3 LYS A  14      10.840   1.865  -0.622  1.00  3.03           H
ATOM    201  HE2 LYS A  14       8.281   1.061  -1.922  1.00  2.86           H
ATOM    202  HE3 LYS A  14       8.166   2.326  -0.684  1.00  2.68           H
ATOM    203  HZ1 LYS A  14      10.324   2.421  -2.695  1.00  4.00           H
ATOM    204  HZ2 LYS A  14       9.627   3.693  -1.807  1.00  3.99           H
ATOM    205  HZ3 LYS A  14       8.776   2.987  -3.097  1.00  3.76           H
ATOM    206  N   SER A  15      10.296  -3.090   1.785  1.00  0.86           N
ATOM    207  CA  SER A  15      10.720  -2.863   3.203  1.00  1.22           C
ATOM    208  C   SER A  15      12.029  -3.609   3.498  1.00  1.64           C
ATOM    209  O   SER A  15      12.918  -3.000   4.074  1.00  2.15           O
ATOM    210  CB  SER A  15       9.576  -3.422   4.055  1.00  1.58           C
ATOM    211  OG  SER A  15       8.902  -2.348   4.703  1.00  2.27           O
ATOM    212  OXT SER A  15      12.121  -4.776   3.146  1.00  2.22           O
ATOM    213  H   SER A  15      10.161  -4.002   1.456  1.00  0.83           H
ATOM    214  HA  SER A  15      10.839  -1.809   3.396  1.00  1.59           H
ATOM    215  HB2 SER A  15       8.879  -3.949   3.426  1.00  1.86           H
ATOM    216  HB3 SER A  15       9.978  -4.104   4.792  1.00  1.97           H
ATOM    217  HG  SER A  15       8.286  -1.961   4.073  1.00  2.68           H
TER     218      SER A  15
MASTER       83    0    0    1    0    0    0    6  217    1    0    2
END
)";

BOOST_AUTO_TEST_CASE(Test_annotated_atomistic_to_cg)
{
    set_default_monomer_db_path();

    auto atomistic_mol = RDKit::v2::FileParsers::MolFromPDBBlock(PDB_1DNG);
    auto cg_mol = atomistic_to_cg(*atomistic_mol);

    BOOST_CHECK_EQUAL(to_string(*cg_mol, Format::HELM),
                      "PEPTIDE1{Q.A.P.A.Y.E.E.A.A.E.E.L.A.K.S}$$$$V2.0");
}

BOOST_AUTO_TEST_CASE(Test_cg_to_atomistic)
{
    // More tests validation tests are available in python, these are to ensure
    // memtest is ran
    {
        auto cg_mol =
            to_rdkit("PEPTIDE1{C.A.A.A.C}$PEPTIDE1,PEPTIDE1,1:R3-5:R3$$$V2.0",
                     Format::HELM);
        auto atomistic_mol = cg_to_atomistic(*cg_mol);
        BOOST_CHECK_EQUAL(::RDKit::MolToSmiles(*atomistic_mol),
                          "C[C@@H]1NC(=O)[C@H](C)NC(=O)[C@@H](N)CSSC[C@@H](C(="
                          "O)O)NC(=O)[C@H](C)NC1=O");
    }
    {
        auto cg_mol = to_rdkit("PEPTIDE1{D.E.F.G}|PEPTIDE2{C.E}$PEPTIDE1,"
                               "PEPTIDE2,2:R3-1:R1$$$V2.0");
        auto atomistic_mol = cg_to_atomistic(*cg_mol);
        BOOST_CHECK_EQUAL(
            ::RDKit::MolToSmiles(*atomistic_mol),
            "N[C@@H](CC(=O)O)C(=O)N[C@@H](CCC(=O)N[C@@H](CS)C(=O)N[C@@H](CCC(="
            "O)O)C(=O)O)C(=O)N[C@@H](Cc1ccccc1)C(=O)NCC(=O)O");
    }
    {
        // error condition -- invalid monomer
        auto cg_mol = to_rdkit("PEPTIDE1{F.Y.Z.G.R.L}$$$$V2.0");
        BOOST_CHECK_THROW(cg_to_atomistic(*cg_mol), std::out_of_range);
    }
    {
        // error condition -- A does not have an R3 attachment point
        auto cg_mol =
            to_rdkit("PEPTIDE1{A.A.A.A.A}$PEPTIDE1,PEPTIDE1,1:R3-5:R3$$$V2.0");
        BOOST_CHECK_THROW(cg_to_atomistic(*cg_mol), std::runtime_error);
    }
    {
        // error condition -- A/C/G/T are the nitrogenous base thus only have 1
        // attachment point and cannot be in the backbone
        auto cg_mol = to_rdkit("RNA1{A.C.G.T}$$$$V2.0");
        BOOST_CHECK_THROW(cg_to_atomistic(*cg_mol), std::runtime_error);
    }
}

BOOST_AUTO_TEST_CASE(Test_reordering_residues)
{
    ::RDKit::RWMol cg_mol;
    add_monomer(cg_mol, "D", 3, "PEPTIDE1");
    add_monomer(cg_mol, "C", 1, "PEPTIDE1");
    add_monomer(cg_mol, "B", 4, "PEPTIDE1");
    add_monomer(cg_mol, "A", 2, "PEPTIDE1");
    add_connection(cg_mol, 3, 2);
    add_connection(cg_mol, 2, 1);
    add_connection(cg_mol, 1, 0);
    assign_chains(cg_mol);

    BOOST_CHECK_EQUAL(to_string(cg_mol, Format::HELM),
                      "PEPTIDE1{A.B.C.D}$$$$V2.0");
}